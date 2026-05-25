import { Entity, Column, PrimaryGeneratedColumn, CreateDateColumn } from 'typeorm';

@Entity()
export class Mail {
  @PrimaryGeneratedColumn()
  id: number;

  @Column()
  content: string;

  @Column({ nullable: true })
  sender: string;

  @Column()
  keyword: string;

  @CreateDateColumn()
  createdAt: Date;

  @Column({ nullable: true })   // 第一次被领取的时间
  retrievedAt: Date;
}