<template>
  <div class="app-layout">
    <!-- 左侧公屏 -->
    <div class="main-area">
      <div class="public-chat">
        <header class="chat-header">当前用户：{{ store.username }}</header>
        <main class="chat-messages" ref="publicMessagesContainer">
          <div v-for="(msg, index) in store.publicMessages" :key="index" class="message-row">
            <div :class="['message-bubble', msg.sender === store.username ? 'self' : 'other']">
              <div class="message-sender">{{ msg.sender }}</div>
              <div class="message-content">{{ msg.content }}</div>
              <div class="message-time">{{ formatTime(msg.time) }}</div>
            </div>
            <button
              v-if="msg.sender !== store.username"
              class="reply-btn"
              @click="startPrivateChat(msg.sender)"
            >
              回复
            </button>
          </div>
          <div v-if="store.publicMessages.length === 0" class="empty-tip">暂无消息，发一条吧~</div>
        </main>
        <footer class="chat-input">
          <input
            v-model="inputContent"
            type="text"
            placeholder="输入公屏消息..."
            @keyup.enter="sendPublic"
          />
          <button @click="sendPublic" :disabled="!inputContent.trim()">发送</button>
        </footer>
      </div>
    </div>

    <!-- 右侧侧边栏：显示激活的私聊会话 -->
    <aside class="sidebar">
      <h3>私聊</h3>
      <div
        v-for="session in store.activeSessions"
        :key="session.partnerId"
        :class="['contact-item', session.hasNew ? 'blink' : '']"
        @click="openPrivateChat(session.partnerId)"
      >
        <div class="avatar">{{ session.partnerId.substring(0,2) }}</div>
        <span class="name">{{ session.partnerId }}</span>
        <span v-if="session.hasNew" class="badge">新</span>
      </div>
      <div v-if="Object.keys(store.activeSessions).length === 0" class="empty-tip">暂无私聊</div>
    </aside>

    <!-- 私聊窗口（Teleport 到 body） -->
    <Teleport to="body">
      <div
        v-for="partnerId in openWindows"
        :key="partnerId"
        class="private-window"
      >
        <template v-if="store.activeSessions[partnerId]">
          <div class="private-header">
            与 {{ partnerId }} 的私聊
            <button class="close-btn" @click="closePrivateChat(partnerId)">关闭</button>
          </div>
          <div class="private-messages" :ref="el => setPrivateMsgRef(partnerId, el)">
            <div
              v-for="(msg, idx) in store.activeSessions[partnerId].messages"
              :key="idx"
              :class="['message-bubble', msg.sender === store.username ? 'self' : 'other']"
            >
              <div class="message-sender">{{ msg.sender }}</div>
              <div class="message-content">{{ msg.content }}</div>
              <div class="message-time">{{ formatTime(msg.time) }}</div>
            </div>
            <div v-if="store.activeSessions[partnerId].messages.length === 0" class="empty-tip">还没有消息</div>
          </div>
          <div class="private-input">
            <input
              v-model="privateInputs[partnerId]"
              type="text"
              placeholder="输入私聊消息..."
              @keyup.enter="sendPrivate(partnerId)"
            />
            <button @click="sendPrivate(partnerId)">发送</button>
          </div>
        </template>
      </div>
    </Teleport>
  </div>
</template>

<script setup lang="ts">
import { ref, reactive, nextTick, watch, onMounted, onUnmounted } from 'vue';
import { useChatStore } from './stores/chat';

const store = useChatStore();
const inputContent = ref('');
const openWindows = ref<string[]>([]);
const privateInputs = reactive<Record<string, string>>({});
const privateMsgRefs: Record<string, HTMLElement> = {};

function setPrivateMsgRef(partnerId: string, el: any) {
  if (el) privateMsgRefs[partnerId] = el;
}

const publicMessagesContainer = ref<HTMLElement>();
watch(() => store.publicMessages.length, async () => {
  await nextTick();
  if (publicMessagesContainer.value) {
    publicMessagesContainer.value.scrollTop = publicMessagesContainer.value.scrollHeight;
  }
});

// 监听 activeSessions 变化自动清理不存在的窗口
watch(() => Object.keys(store.activeSessions).length, () => {
  const existingIds = Object.keys(store.activeSessions);
  openWindows.value = openWindows.value.filter(id => existingIds.includes(id));
  for (const id of Object.keys(privateInputs)) {
    if (!existingIds.includes(id)) {
      delete privateInputs[id];
      delete privateMsgRefs[id];
    }
  }
});

// 监听消息滚动
watch(store.activeSessions, async () => {
  await nextTick();
  for (const partnerId in privateMsgRefs) {
    const el = privateMsgRefs[partnerId];
    if (el) el.scrollTop = el.scrollHeight;
  }
}, { deep: true });

function formatTime(isoStr: string) {
  return new Date(isoStr).toLocaleTimeString('zh-CN', { hour12: false });
}

function sendPublic() {
  if (!inputContent.value.trim()) return;
  store.sendPublicMessage(inputContent.value);
  inputContent.value = '';
}

function startPrivateChat(targetId: string) {
  // 如果会话已存在，直接打开（置顶）
  if (store.activeSessions[targetId]) {
    openPrivateChat(targetId);
    return;
  }
  // 不存在则发起邀请，并创建本地会话
  store.sendPrivateInvite(targetId);
  store.activeSessions[targetId] = { partnerId: targetId, messages: [], hasNew: false };
  openPrivateChat(targetId);
}

function openPrivateChat(partnerId: string) {
  // 将窗口提到最前（添加到数组并去重）
  openWindows.value = openWindows.value.filter(id => id !== partnerId);
  openWindows.value.push(partnerId);  // 最新添加的在最后，我们通过 z-index 控制置顶? 简单做法：重新 push 后新窗口会靠后渲染，由于都是 fixed，后面的会覆盖前面的。我们可以控制 z-index 递增，但这里先简化。
  if (store.activeSessions[partnerId]) {
    store.activeSessions[partnerId].hasNew = false;
  }
  if (!privateInputs[partnerId]) {
    privateInputs[partnerId] = '';
  }
}

function sendPrivate(partnerId: string) {
  const content = privateInputs[partnerId];
  if (!content?.trim()) return;
  store.sendPrivateMessage(partnerId, content);
  privateInputs[partnerId] = '';
}

function closePrivateChat(partnerId: string) {
  store.closeSession(partnerId);
  openWindows.value = openWindows.value.filter(id => id !== partnerId);
  delete privateInputs[partnerId];
  delete privateMsgRefs[partnerId];
}

onMounted(() => {
  store.connect();
});

onUnmounted(() => {
  store.disconnect();
});
</script>

<style scoped>
/* 原有样式保持不变 */
.app-layout { display: flex; height: 100vh; max-width: 900px; margin: 0 auto; }
.main-area { flex: 1; display: flex; flex-direction: column; }
.public-chat { display: flex; flex-direction: column; height: 100%; }
.chat-header { background: #4caf50; color: white; padding: 10px; }
.chat-messages { flex:1; overflow-y: auto; padding:10px; background:#f5f5f5; }
.message-row { display: flex; align-items: center; margin-bottom: 8px; }
.message-bubble { max-width: 70%; margin-bottom: 4px; }
.message-bubble.self { margin-left: auto; background:#dcf8c6; border-radius:8px; padding:4px 8px; }
.message-bubble.other { margin-right: auto; background:white; border-radius:8px; padding:4px 8px; }
.message-sender { font-size: 12px; color:#666; }
.message-content { word-break: break-word; }
.message-time { font-size:10px; color:#999; }
.reply-btn { margin-left: 8px; padding: 2px 6px; font-size: 12px; cursor: pointer; }
.empty-tip { text-align:center; color:#aaa; margin-top:40px; }
.chat-input { display:flex; padding:8px; background:white; border-top:1px solid #ddd; }
.chat-input input { flex:1; padding:8px; border:1px solid #ddd; border-radius:4px; margin-right:8px; }
.chat-input button { padding:8px 16px; background:#4caf50; color:white; border:none; border-radius:4px; }
.chat-input button:disabled { background:#ccc; }
.sidebar { width: 160px; background: #f9f9f9; border-left: 1px solid #ddd; padding: 10px; overflow-y: auto; }
.contact-item { display: flex; align-items: center; padding: 6px; margin-bottom: 6px; cursor: pointer; background: white; border-radius: 4px; }
.contact-item.blink { animation: blink-animation 0.8s infinite alternate; }
@keyframes blink-animation { from { background: #ffe0b2; } to { background: #ffcc80; } }
.avatar { width: 28px; height: 28px; background: #4caf50; color: white; border-radius: 50%; display: flex; align-items: center; justify-content: center; margin-right: 6px; font-size: 12px; }
.badge { color: red; font-size: 12px; margin-left: auto; }
.private-window { position: fixed; bottom: 20px; right: 200px; width: 300px; height: 350px; background: white; border: 1px solid #ccc; border-radius: 8px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); display: flex; flex-direction: column; z-index: 100; }
.private-header { background: #2196f3; color: white; padding: 6px 10px; border-radius: 8px 8px 0 0; display: flex; justify-content: space-between; align-items: center; }
.close-btn { background: none; border: none; color: white; cursor: pointer; font-size: 16px; }
.private-messages { flex: 1; overflow-y: auto; padding: 8px; background: #fafafa; }
.private-input { display: flex; padding: 6px; border-top: 1px solid #ddd; }
.private-input input { flex: 1; padding: 4px; border: 1px solid #ddd; border-radius: 4px; margin-right: 4px; }
.private-input button { padding: 4px 12px; background: #2196f3; color: white; border: none; border-radius: 4px; }
</style>