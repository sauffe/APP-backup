import { Controller, Post, Body } from '@nestjs/common';
import { MailService } from './mail.service';

@Controller('mail')
export class MailController {
  constructor(private readonly mailService: MailService) {}

  @Post('send')
  async send(@Body('content') content: string, @Body('keyword') keyword: string) {
    return this.mailService.send(content, keyword);
  }

  @Post('retrieve')
  async retrieve(@Body('keyword') keyword: string) {
    const result = await this.mailService.retrieve(keyword);
    if (!result) {
      return { message: '没有找到相关信件，或信件已失效' };
    }
    return result;
  }
}