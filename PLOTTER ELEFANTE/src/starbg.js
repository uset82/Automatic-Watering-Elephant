// Animated star and comet background for #star-bg canvas
const canvas = document.getElementById('star-bg');
const ctx = canvas.getContext('2d');
let w = window.innerWidth;
let h = window.innerHeight;
canvas.width = w;
canvas.height = h;

function resize() {
  w = window.innerWidth;
  h = window.innerHeight;
  canvas.width = w;
  canvas.height = h;
}
window.addEventListener('resize', resize);

// Star and comet objects
const STAR_COUNT = 80; // fewer stars for subtlety
const COMET_COUNT = 1; // single slow comet
const stars = [];
const comets = [];

function randomStar() {
  return {
    x: Math.random() * w,
    y: Math.random() * h,
    r: Math.random() * 1.0 + 0.4,
    alpha: Math.random() * 0.4 + 0.5,
    speed: Math.random() * 0.08 + 0.02,
    twinkle: Math.random() * 0.03 + 0.005,
    color: `rgba(180,140,255,1)`
  };
}
for (let i = 0; i < STAR_COUNT; i++) stars.push(randomStar());

function randomComet() {
  return {
    x: Math.random() * w,
    y: Math.random() * h * 0.5,
    vx: Math.random() * 1.0 + 1.2,
    vy: Math.random() * 0.3 - 0.12,
    len: Math.random() * 60 + 40,
    tail: Math.random() * 12 + 8,
    color: 'rgba(200,160,255,0.85)',
    alpha: 0.95,
    life: Math.random() * 300 + 220
  };
}
for (let i = 0; i < COMET_COUNT; i++) comets.push(randomComet());

function drawStars() {
  for (const s of stars) {
    s.alpha += (Math.random()-0.5)*s.twinkle;
    s.alpha = Math.max(0.25, Math.min(0.95, s.alpha));
    ctx.beginPath();
    ctx.arc(s.x, s.y, s.r, 0, Math.PI*2);
    ctx.fillStyle = `rgba(180,140,255,${s.alpha})`;
    ctx.shadowColor = 'rgba(150,110,230,0.8)';
    ctx.shadowBlur = 6 * s.r;
    ctx.fill();
    ctx.shadowBlur = 0;
    s.x += s.speed;
    if (s.x > w + 10) s.x = -10;
  }
}
function drawComets() {
  for (const c of comets) {
    ctx.save();
    ctx.globalAlpha = c.alpha;
    // head
    ctx.beginPath();
    ctx.arc(c.x, c.y, 4.5, 0, Math.PI*2);
    ctx.fillStyle = c.color;
    ctx.shadowColor = 'rgba(220,200,255,0.9)';
    ctx.shadowBlur = 12;
    ctx.fill();
    ctx.shadowBlur = 0;
    // softer tail
    for (let t = 0; t < c.tail; t++) {
      const alpha = 0.12 * (1 - t / c.tail);
      ctx.beginPath();
      ctx.ellipse(c.x - t * 5, c.y - t * 2.2, Math.max(1, 5 - t*0.35), Math.max(0.6, 3 - t*0.2), 0, 0, Math.PI*2);
      ctx.fillStyle = `rgba(200,160,255,${alpha})`;
      ctx.fill();
    }
    ctx.restore();
    c.x += c.vx;
    c.y += c.vy;
    c.life--;
    if (c.x > w + 60 || c.life < 0) {
      Object.assign(c, randomComet());
      c.x = -60;
    }
  }
}
function animate() {
  ctx.clearRect(0,0,w,h);
  drawStars();
  drawComets();
  requestAnimationFrame(animate);
}
animate();
