const header = document.querySelector('.site-header');
const reveals = document.querySelectorAll('.reveal');
const video = document.querySelector('#gameVideo');
const placeholder = document.querySelector('#videoPlaceholder');
const videoCorner = document.querySelector('.video-corner');

function updateHeader() {
  header.classList.toggle('scrolled', window.scrollY > 40);
}

updateHeader();
window.addEventListener('scroll', updateHeader, { passive: true });

const revealObserver = new IntersectionObserver((entries) => {
  entries.forEach((entry) => {
    if (!entry.isIntersecting) return;
    entry.target.classList.add('visible');
    revealObserver.unobserve(entry.target);
  });
}, { threshold: 0.12, rootMargin: '0px 0px -45px' });

reveals.forEach((element) => revealObserver.observe(element));

// 当“视频/游戏演示.mp4”存在时，浏览器会自动隐藏占位提示并显示播放器。
video.addEventListener('loadedmetadata', () => {
  placeholder.classList.add('hidden');
  const minutes = Math.floor(video.duration / 60).toString().padStart(2, '0');
  const seconds = Math.floor(video.duration % 60).toString().padStart(2, '0');
  videoCorner.textContent = `GAMEPLAY · ${minutes}:${seconds}`;
});

video.addEventListener('error', () => {
  placeholder.classList.remove('hidden');
  videoCorner.textContent = 'COMING SOON · 00:00';
});
