import { Injectable } from '@nestjs/common';
import { InjectRepository } from '@nestjs/typeorm';
import { Repository, LessThan, MoreThan, IsNull } from 'typeorm';
import { Mail } from './mail.entity';
import { Cron, CronExpression } from '@nestjs/schedule';

@Injectable()
export class MailService {
  constructor(
    @InjectRepository(Mail)
    private readonly mailRepository: Repository<Mail>,
  ) {}

  // 投递信件
  async send(content: string, keyword: string, sender?: string): Promise<Mail> {
    const mail = this.mailRepository.create({ content, keyword, sender });
    return this.mailRepository.save(mail);
  }

  // 根据关键词取信
  async retrieve(keyword: string): Promise<{ content: string; createdAt: Date; remaining?: number } | null> {
    // 查找关键词匹配且未被彻底销毁的信件
    // 条件：retrievedAt 为空（未领取）或者 retrievedAt 在5分钟内（有效期内）
    const fiveMinutesAgo = new Date(Date.now() - 5 * 60 * 1000);

    const mail = await this.mailRepository.findOne({
      where: [
        { keyword, retrievedAt: IsNull() },                          // 未领取
        { keyword, retrievedAt: MoreThan(fiveMinutesAgo) },          // 已领取但还在5分钟内
      ],
      order: { createdAt: 'DESC' },
    });

    if (!mail) {
      return null; // 没有匹配的信件或已失效
    }

    // 如果是第一次领取，记录时间
    if (!mail.retrievedAt) {
      mail.retrievedAt = new Date();
      await this.mailRepository.save(mail);
    }

    // 计算剩余有效时间（秒）
    const expiresAt = new Date(mail.retrievedAt.getTime() + 5 * 60 * 1000);
    const remaining = Math.max(0, Math.floor((expiresAt.getTime() - Date.now()) / 1000));

    return {
      content: mail.content,
      createdAt: mail.createdAt,
      remaining,
    };
  }

  // 定时清理已失效的信件（retrievedAt + 5分钟 < 现在）
  @Cron(CronExpression.EVERY_MINUTE)
  async cleanExpiredMails() {
    const fiveMinutesAgo = new Date(Date.now() - 5 * 60 * 1000);
    await this.mailRepository.delete({
      retrievedAt: LessThan(fiveMinutesAgo),
    });
  }
}