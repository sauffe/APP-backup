import { Module } from '@nestjs/common';
import { ChatGateway } from './chat.gateway';
import { redisProvider } from './redis.provider';

@Module({
  providers: [ChatGateway, redisProvider],
})
export class ChatModule {}