// src/chat/redis.provider.ts
import { Provider } from '@nestjs/common';
import Redis from 'ioredis';

export const REDIS_CLIENT = 'REDIS_CLIENT';

export const redisProvider: Provider = {
  provide: REDIS_CLIENT,
  useFactory: () => {
    return new Redis({
      host: 'localhost',
      port: 6379,
      // 如果 Redis 有密码，在这里配 password
    });
  },
};