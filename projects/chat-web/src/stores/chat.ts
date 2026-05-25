import { defineStore } from 'pinia';
import { ref, reactive } from 'vue';

interface ChatMessage {
  sender: string;
  content: string;
  time: string;
}

interface PrivateSession {
  partnerId: string;
  messages: ChatMessage[];
  hasNew: boolean;
}

export const useChatStore = defineStore('chat', () => {
  const username = ref('');
  const publicMessages = reactive<ChatMessage[]>([]);
  const activeSessions = reactive<Record<string, PrivateSession>>({});
  let socket: WebSocket | null = null;

  function connect() {
    socket = new WebSocket('ws://localhost:3000');
    socket.onmessage = (event) => {
      try {
        const msg = JSON.parse(event.data);
        const { event: ev, data } = msg;
        if (ev === 'username') {
          username.value = data.username;
        } else if (ev === 'public_message') {
          publicMessages.push(data);
        } else if (ev === 'private_invite') {
          const from = data.from;
          if (!activeSessions[from]) {
            activeSessions[from] = { partnerId: from, messages: [], hasNew: true };
          } else {
            activeSessions[from].hasNew = true;
          }
        } else if (ev === 'private_message') {
          const { from, to, content, time } = data;
          const partner = from === username.value ? to : from;
          if (!activeSessions[partner]) {
            activeSessions[partner] = { partnerId: partner, messages: [], hasNew: true };
          }
          activeSessions[partner].messages.push({ sender: from, content, time });
          activeSessions[partner].hasNew = true;
        } else if (ev === 'private_close') {
          const { from, to } = data;
          const partner = from === username.value ? to : from;
          delete activeSessions[partner];
        } else if (ev === 'error') {
          console.error('服务器错误：', data.message);
        }
      } catch (e) {
        console.error('消息解析失败', e);
      }
    };
    socket.onclose = () => console.log('WebSocket 断开');
  }

  function sendPublicMessage(content: string) {
    if (socket && socket.readyState === WebSocket.OPEN && content.trim()) {
      socket.send(JSON.stringify({ event: 'public_message', data: { content } }));
    }
  }

  function sendPrivateInvite(to: string) {
    if (socket && socket.readyState === WebSocket.OPEN) {
      socket.send(JSON.stringify({ event: 'private_invite', data: { to } }));
    }
  }

  function sendPrivateMessage(to: string, content: string) {
    if (socket && socket.readyState === WebSocket.OPEN && content.trim()) {
      socket.send(JSON.stringify({ event: 'private_message', data: { to, content } }));
    }
  }

  function closeSession(to: string) {
    if (socket && socket.readyState === WebSocket.OPEN) {
      socket.send(JSON.stringify({ event: 'private_close', data: { to } }));
    }
    delete activeSessions[to];
  }

  function disconnect() {
    if (socket) {
      socket.close();
      socket = null;
    }
  }

  return {
    username,
    publicMessages,
    activeSessions,
    connect,
    sendPublicMessage,
    sendPrivateInvite,
    sendPrivateMessage,
    closeSession,
    disconnect,
  };
});