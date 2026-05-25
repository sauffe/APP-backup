<template>
  <n-config-provider :theme="theme">
    <div class="app-container">
      <!-- 左侧聊天室 70% -->
      <div class="chat-area">
        <header class="chat-header">当前用户：{{ store.username }}</header>
        <main class="chat-messages" ref="publicMessagesContainer">
          <div v-for="(msg, index) in store.publicMessages" :key="index" class="message-row">
            <div :class="['message-bubble', msg.sender === store.username ? 'self' : 'other']">
              <div class="message-sender">{{ msg.sender }}</div>
              <div class="message-content">{{ msg.content }}</div>
              <div class="message-time">{{ formatTime(msg.time) }}</div>
            </div>
            <n-button
              v-if="msg.sender !== store.username"
              size="tiny"
              text
              @click="startPrivateChat(msg.sender)"
              style="margin-left: 8px"
            >
              回复
            </n-button>
          </div>
          <div v-if="store.publicMessages.length === 0" class="empty-tip">暂无消息，发一条吧~</div>
        </main>
        <footer class="chat-input">
          <n-input
            v-model:value="inputContent"
            placeholder="输入公屏消息..."
            :maxlength="500"
            clearable
            @keyup.enter="sendPublic"
          >
            <template #prefix>💬</template>
          </n-input>
          <n-button type="primary" @click="sendPublic" :disabled="!inputContent.trim()">发送</n-button>
        </footer>
      </div>

      <!-- 右侧侧边栏 30% -->
      <div class="sidebar">
        <header class="sidebar-header">
          <n-button type="primary" dashed block @click="mode = mode === 'mail' ? 'chat' : 'mail'">
            {{ mode === 'mail' ? '📮 匿名投信' : '💬 聊天室' }}
          </n-button>
        </header>
        <div class="sidebar-content">
          <!-- 信箱模式 -->
          <div v-if="mode === 'mail'">
            <MailBox />
          </div>
          <!-- 私聊会话列表模式 -->
          <div v-else>
            <h4>私聊会话</h4>
            <div
              v-for="session in store.activeSessions"
              :key="session.partnerId"
              :class="['contact-item', session.hasNew ? 'blink' : '']"
              @click="openPrivateChat(session.partnerId)"
            >
              <n-avatar :size="28" style="margin-right: 8px">{{ session.partnerId.substring(0,2) }}</n-avatar>
              <span>{{ session.partnerId }}</span>
              <n-badge v-if="session.hasNew" dot style="margin-left: auto" />
            </div>
            <div v-if="Object.keys(store.activeSessions).length === 0" class="empty-tip">暂无私聊</div>
          </div>
        </div>
      </div>
    </div>

    <!-- 私聊浮动窗口（固定在右下角，最高层级） -->
    <Teleport to="body">
      <div v-for="partnerId in openWindows" :key="partnerId" class="private-window">
        <template v-if="store.activeSessions[partnerId]">
          <div class="private-header">
            与 {{ partnerId }} 的私聊
            <n-button text @click="closePrivateChat(partnerId)">关闭</n-button>
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
            <n-input
              v-model:value="privateInputs[partnerId]"
              placeholder="输入私聊消息..."
              @keyup.enter="sendPrivate(partnerId)"
            />
            <n-button type="primary" size="small" @click="sendPrivate(partnerId)">发送</n-button>
          </div>
        </template>
      </div>
    </Teleport>
  </n-config-provider>
</template>

<script setup lang="ts">
import { ref, reactive, nextTick, watch, onMounted, onUnmounted } from 'vue';
import { useChatStore } from './stores/chat';
import MailBox from './components/MailBox.vue';

const store = useChatStore();
const inputContent = ref('');
const openWindows = ref<string[]>([]);
const privateInputs = reactive<Record<string, string>>({});
const privateMsgRefs: Record<string, HTMLElement> = {};
const mode = ref<'mail' | 'chat'>('mail');   // 默认显示信箱
const theme = ref(null);                     // 浅色主题

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

// 自动清理已经被关闭的私聊窗口
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

// 私聊消息自动滚动
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
  if (store.activeSessions[targetId]) {
    openPrivateChat(targetId);
    return;
  }
  store.sendPrivateInvite(targetId);
  store.activeSessions[targetId] = { partnerId: targetId, messages: [], hasNew: false };
  openPrivateChat(targetId);
}

function openPrivateChat(partnerId: string) {
  openWindows.value = openWindows.value.filter(id => id !== partnerId);
  openWindows.value.push(partnerId);
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

onMounted(() => store.connect());
onUnmounted(() => store.disconnect());
</script>

<style scoped>
/* 主布局 */
.app-container {
  display: flex;
  height: 100vh;
  width: 100%;
}

/* 左侧聊天室 */
.chat-area {
  flex: 7;
  display: flex;
  flex-direction: column;
  background: #f5f5f5;
}
.chat-header {
  background: #4caf50;
  color: white;
  padding: 10px;
  font-weight: bold;
}
.chat-messages {
  flex: 1;
  overflow-y: auto;
  padding: 16px;
}
.chat-input {
  display: flex;
  align-items: center;
  padding: 12px;
  background: white;
  border-top: 1px solid #ddd;
}
.chat-input .n-input {
  flex: 1;
  margin-right: 12px;
}

/* 消息气泡 */
.message-row {
  display: flex;
  align-items: flex-start;
  margin-bottom: 8px;
}
.message-bubble {
  max-width: 70%;
  padding: 8px 12px;
  border-radius: 16px;
  word-break: break-word;
}
.message-bubble.self {
  margin-left: auto;
  background: #dcf8c6;
  border-bottom-right-radius: 4px;
}
.message-bubble.other {
  margin-right: auto;
  background: white;
  border-bottom-left-radius: 4px;
}
.message-sender { font-size: 12px; color: #666; margin-bottom: 4px; }
.message-content { font-size: 14px; }
.message-time { font-size: 10px; color: #999; text-align: right; margin-top: 2px; }
.empty-tip { text-align: center; color: #aaa; margin-top: 40px; }

/* 右侧侧边栏 */
.sidebar {
  flex: 3;
  display: flex;
  flex-direction: column;
  background: #fff;
  border-left: 1px solid #eee;
}
.sidebar-header {
  padding: 12px;
  border-bottom: 1px solid #eee;
}
.sidebar-content {
  flex: 1;
  overflow-y: auto;
  padding: 12px;
}

/* 联系人条目 */
.contact-item {
  display: flex;
  align-items: center;
  padding: 8px;
  margin-bottom: 4px;
  cursor: pointer;
  border-radius: 6px;
  transition: background 0.2s;
}
.contact-item:hover { background: #f0f0f0; }
.contact-item.blink { animation: blink 0.8s infinite alternate; }
@keyframes blink { from { background: #fff3e0; } to { background: #ffe0b2; } }

/* 私聊浮动窗口（最强层级，安全位置） */
.private-window {
  position: fixed;
  bottom: 20px;
  right: 20px;
  width: 360px;
  height: 450px;
  background: white;
  border: 1px solid #ccc;
  border-radius: 8px;
  box-shadow: 0 2px 10px rgba(0,0,0,0.15);
  display: flex;
  flex-direction: column;
  z-index: 10000;            /* 确保不会被任何元素遮挡 */
}
.private-header {
  background: #2196f3;
  color: white;
  padding: 8px 12px;
  border-radius: 8px 8px 0 0;
  display: flex;
  justify-content: space-between;
  align-items: center;
}
.private-messages {
  flex: 1;
  overflow-y: auto;
  padding: 8px;
  background: #fafafa;
}
.private-input {
  display: flex;
  padding: 8px;
  border-top: 1px solid #ddd;
  gap: 8px;
}
</style>