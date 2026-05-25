import { createApp } from 'vue';
import { createPinia } from 'pinia';
import App from './App.vue';
import naive from 'naive-ui';   // 确保是 naive 不是 NaiveUI

const app = createApp(App);
app.use(createPinia());
app.use(naive);
app.mount('#app');