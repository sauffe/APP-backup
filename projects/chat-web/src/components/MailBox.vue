<template>
  <div class="mailbox">
    <!-- 投递卡片不变 -->
    <n-card title="📥 投递匿名信" size="small">
      <n-input v-model:value="sendKeyword" placeholder="设置关键词（取信时需要）" style="margin-bottom: 12px" />
      <n-input v-model:value="sendContent" type="textarea" placeholder="写下信件内容..." :autosize="{ minRows: 3, maxRows: 5 }" />
      <n-button type="primary" block style="margin-top: 12px" @click="handleSend" :loading="sending" :disabled="!sendContent.trim() || !sendKeyword.trim()">
        投递
      </n-button>
    </n-card>

    <!-- 领取卡片 -->
    <n-card title="📬 领取信件" size="small" style="margin-top: 16px">
      <n-input v-model:value="retrieveKeyword" placeholder="输入关键词领取信件" style="margin-bottom: 12px" />
      <n-button type="primary" block @click="handleRetrieve" :loading="retrieving" :disabled="!retrieveKeyword.trim()">
        领取
      </n-button>
      <div v-if="retrievedContent !== null" class="retrieved-letter">
        <p class="letter-content">{{ retrievedContent }}</p>
        <p class="letter-time" v-if="retrievedTime">投递时间：{{ retrievedTime }}</p>
        <p class="letter-time" v-if="remaining !== null">剩余有效时间：{{ remaining }} 秒</p>
        <span v-if="retrievedContent === ''" class="empty">没有找到信件，检查关键词或已失效</span>
      </div>
    </n-card>
  </div>
</template>

<script setup lang="ts">
import { ref } from 'vue';
import axios from 'axios';

const sendContent = ref('');
const sendKeyword = ref('');
const sending = ref(false);

const retrieveKeyword = ref('');
const retrieving = ref(false);
const retrievedContent = ref<string | null>(null);
const retrievedTime = ref<string | null>(null);
const remaining = ref<number | null>(null);

async function handleSend() {
  sending.value = true;
  try {
    await axios.post('http://localhost:3000/mail/send', {
      content: sendContent.value,
      keyword: sendKeyword.value,
    });
    sendContent.value = '';
    sendKeyword.value = '';
    alert('投递成功');
  } catch (e) {
    console.error(e);
  } finally {
    sending.value = false;
  }
}

async function handleRetrieve() {
  retrieving.value = true;
  retrievedContent.value = null;
  retrievedTime.value = null;
  remaining.value = null;
  try {
    const res = await axios.post('http://localhost:3000/mail/retrieve', {
      keyword: retrieveKeyword.value,
    });
    if (res.data.content) {
      retrievedContent.value = res.data.content;
      retrievedTime.value = res.data.createdAt
        ? new Date(res.data.createdAt).toLocaleTimeString('zh-CN', { hour12: false })
        : null;
      remaining.value = res.data.remaining;
    } else {
      retrievedContent.value = ''; // 表示未找到
    }
  } catch (e) {
    console.error(e);
  } finally {
    retrieving.value = false;
  }
}
</script>

<style scoped>
/* 样式保持不变 */
.retrieved-letter {
  margin-top: 12px;
  padding: 8px;
  background: #f6ffed;
  border: 1px solid #b7eb8f;
  border-radius: 4px;
}
.letter-content {
  margin: 0;
  word-break: break-word;
}
.letter-time {
  font-size: 12px;
  color: #888;
}
.empty {
  color: #999;
}
</style>