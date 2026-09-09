const header = document.querySelector('.site-header');
const reveals = document.querySelectorAll('.reveal');

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

const carousel = document.querySelector('[data-carousel]');

if (carousel) {
  const slides = [...carousel.querySelectorAll('.carousel-slide')];
  const dotsContainer = carousel.querySelector('.carousel-dots');
  const counter = carousel.querySelector('.carousel-current');
  const total = carousel.querySelector('.carousel-total');
  const previousButton = carousel.querySelector('.carousel-prev');
  const nextButton = carousel.querySelector('.carousel-next');
  const reduceMotion = window.matchMedia('(prefers-reduced-motion: reduce)').matches;
  let currentIndex = 0;
  let autoplayTimer = null;
  let touchStartX = 0;

  // 根据 .carousel-slide 的数量自动生成导航圆点；新增图片时无需再修改页码。
  dotsContainer.replaceChildren();
  const dots = slides.map((_, index) => {
    const dot = document.createElement('button');
    dot.type = 'button';
    dot.setAttribute('aria-label', `查看第${index + 1}张`);
    dotsContainer.appendChild(dot);
    return dot;
  });
  total.textContent = String(slides.length).padStart(2, '0');

  function showSlide(index) {
    currentIndex = (index + slides.length) % slides.length;

    slides.forEach((slide, slideIndex) => {
      const isActive = slideIndex === currentIndex;
      slide.classList.toggle('is-active', isActive);
      slide.setAttribute('aria-hidden', String(!isActive));
    });

    dots.forEach((dot, dotIndex) => {
      const isActive = dotIndex === currentIndex;
      dot.classList.toggle('is-active', isActive);
      if (isActive) dot.setAttribute('aria-current', 'true');
      else dot.removeAttribute('aria-current');
    });

    counter.textContent = String(currentIndex + 1).padStart(2, '0');
  }

  function stopAutoplay() {
    window.clearInterval(autoplayTimer);
    autoplayTimer = null;
  }

  function startAutoplay() {
    stopAutoplay();
    if (!reduceMotion && !document.hidden &&
        !carousel.matches(':hover') && !carousel.contains(document.activeElement)) {
      autoplayTimer = window.setInterval(() => showSlide(currentIndex + 1), 4800);
    }
  }

  function navigateTo(index) {
    showSlide(index);
    startAutoplay();
  }

  previousButton.addEventListener('click', () => navigateTo(currentIndex - 1));
  nextButton.addEventListener('click', () => navigateTo(currentIndex + 1));
  dots.forEach((dot, index) => dot.addEventListener('click', () => navigateTo(index)));

  carousel.addEventListener('keydown', (event) => {
    if (event.key === 'ArrowLeft') navigateTo(currentIndex - 1);
    if (event.key === 'ArrowRight') navigateTo(currentIndex + 1);
  });
  carousel.addEventListener('mouseenter', stopAutoplay);
  carousel.addEventListener('mouseleave', startAutoplay);
  carousel.addEventListener('focusin', stopAutoplay);
  carousel.addEventListener('focusout', startAutoplay);
  carousel.addEventListener('touchstart', (event) => {
    touchStartX = event.changedTouches[0].clientX;
    stopAutoplay();
  }, { passive: true });
  carousel.addEventListener('touchend', (event) => {
    const distance = event.changedTouches[0].clientX - touchStartX;
    if (Math.abs(distance) > 45) navigateTo(currentIndex + (distance < 0 ? 1 : -1));
    else startAutoplay();
  }, { passive: true });
  document.addEventListener('visibilitychange', () => {
    if (document.hidden) stopAutoplay();
    else startAutoplay();
  });

  showSlide(0);
  startAutoplay();
}
