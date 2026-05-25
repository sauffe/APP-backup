import {
  WebSocketGateway,
  WebSocketServer,
  OnGatewayConnection,
  OnGatewayDisconnect,
  SubscribeMessage,
} from '@nestjs/websockets';
import { Inject, Injectable } from '@nestjs/common';
import { Server, WebSocket } from 'ws';
import { v4 as uuid } from 'uuid';
import Redis from 'ioredis';
import { REDIS_CLIENT } from './redis.provider';

interface Client extends WebSocket {
  username?: string;
}

@Injectable()
@WebSocketGateway({ cors: true })
export class ChatGateway implements OnGatewayConnection, OnGatewayDisconnect {
  @WebSocketServer()
  server: Server;

  // 维护 username -> client 映射，用于点对点转发
  private clientsMap = new Map<string, Client>();

  constructor(@Inject(REDIS_CLIENT) private readonly redis: Redis) {}

  // ========== 连接 / 断开 ==========
  handleConnection(client: Client) {
    const username = `User_${uuid().substring(0, 6)}`;
    client.username = username;
    this.clientsMap.set(username, client);
    client.send(JSON.stringify({ event: 'username', data: { username } }));
  }

  async handleDisconnect(client: Client) {
    const username = client.username;
    if (!username) return;

    // 清理该用户的所有私聊映射，并通知对方
    const partners = await this.redis.smembers(`session:${username}`);
    for (const partner of partners) {
      await this.redis.srem(`session:${partner}`, username);
      const partnerClient = this.clientsMap.get(partner);
      if (partnerClient && partnerClient.readyState === WebSocket.OPEN) {
        partnerClient.send(JSON.stringify({
          event: 'private_close',
          data: { from: username, to: partner },
        }));
      }
    }
    await this.redis.del(`session:${username}`);
    this.clientsMap.delete(username);
  }

  // ========== 公屏消息（完全解耦，无拦截）==========
  @SubscribeMessage('public_message')
  handlePublicMessage(client: Client, payload: { content: string }) {
    const username = client.username;
    if (!username || !payload.content) return;

    const message = {
      sender: username,
      content: payload.content,
      time: new Date().toISOString(),
    };

    // 广播给所有连接
    this.server.clients.forEach((c) => {
      if (c.readyState === WebSocket.OPEN) {
        c.send(JSON.stringify({ event: 'public_message', data: message }));
      }
    });
  }

  // ========== 私聊邀请（建立映射 + 转发）==========
  @SubscribeMessage('private_invite')
  async handlePrivateInvite(client: Client, payload: { from: string; to: string }) {
    const from = client.username;   // 自己的用户名
    const to = payload.to;
    if (!from || !to) return;

    // 建立双向会话映射
    await this.redis.sadd(`session:${from}`, to);
    await this.redis.sadd(`session:${to}`, from);

    // 将邀请事件转发给 to 用户
    const targetClient = this.clientsMap.get(to);
    if (targetClient && targetClient.readyState === WebSocket.OPEN) {
      targetClient.send(JSON.stringify({
        event: 'private_invite',
        data: { from, to },
      }));
    }
  }

  // ========== 私聊消息（验证后转发）==========
  @SubscribeMessage('private_message')
  async handlePrivateMessage(client: Client, payload: { from: string; to: string; content: string }) {
    const from = client.username;
    const { to, content } = payload;
    if (!from || !to || !content) return;

    // 验证双方是否都在对方的会话集合中
    const fromHasTo = await this.redis.sismember(`session:${from}`, to);
    const toHasFrom = await this.redis.sismember(`session:${to}`, from);
    if (!fromHasTo || !toHasFrom) {
      client.send(JSON.stringify({
        event: 'error',
        data: { message: '私聊会话不存在，请先发起邀请' },
      }));
      return;
    }

    const message = { from, to, content, time: new Date().toISOString() };

    // 转发给接收者
    const targetClient = this.clientsMap.get(to);
    if (targetClient && targetClient.readyState === WebSocket.OPEN) {
      targetClient.send(JSON.stringify({ event: 'private_message', data: message }));
    }
    // 同时回传给发送者（方便本地显示）
    client.send(JSON.stringify({ event: 'private_message', data: message }));
  }

  // ========== 关闭私聊 ==========
  @SubscribeMessage('private_close')
  async handlePrivateClose(client: Client, payload: { from: string; to: string }) {
    const from = client.username;
    const to = payload.to;
    if (!from || !to) return;

    // 删除映射
    await this.redis.srem(`session:${from}`, to);
    await this.redis.srem(`session:${to}`, from);

    // 通知对方私聊已关闭
    const targetClient = this.clientsMap.get(to);
    if (targetClient && targetClient.readyState === WebSocket.OPEN) {
      targetClient.send(JSON.stringify({
        event: 'private_close',
        data: { from, to },
      }));
    }
  }
}