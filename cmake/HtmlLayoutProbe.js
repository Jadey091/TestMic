import { spawn } from "node:child_process";
import { resolve } from "node:path";

const endpoint = process.argv[2] || "http://127.0.0.1:9231";
const debugPort = new URL(endpoint).port || "9231";
const edge = process.argv[3] ||
  "C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe";
const profile = resolve(process.argv[4] || `build-final/layout-cdp-profile-${process.pid}`);
const reference = process.argv[5] || "http://127.0.0.1:8765/index.html#model";

const browser = spawn(edge, ["--headless=new", "--disable-gpu",
  "--no-first-run", "--no-default-browser-check",
  `--remote-debugging-port=${debugPort}`, "--remote-allow-origins=*",
  `--user-data-dir=${profile}`, "--window-size=934,773", reference],
  { stdio: "ignore" });

let version;
for (let attempt = 0; attempt < 100 && !version; attempt++) {
  try { version = await (await fetch(`${endpoint}/json/version`)).json(); }
  catch { await new Promise(resolve => setTimeout(resolve, 100)); }
}
if (!version) {
  browser.kill();
  throw new Error("Browser debug endpoint did not start");
}

await new Promise(resolve => setTimeout(resolve, 800));
const targets = await (await fetch(`${endpoint}/json`)).json();
const target = targets.find(item => item.type === "page" &&
  item.url.includes("127.0.0.1:8765"));
if (!target) throw new Error("No browser page target found");

const socket = new WebSocket(target.webSocketDebuggerUrl);
const expression = String.raw`(async () => {
  const delay = ms => new Promise(resolve => setTimeout(resolve, ms));
  for (let attempt = 0; attempt < 50 &&
    !document.querySelector('[data-page="2"]'); attempt++) await delay(100);
  const snapshot = selectors => Object.fromEntries(selectors.map(selector => [
    selector, [...document.querySelectorAll(selector)].map(element => {
      const rect = element.getBoundingClientRect();
      const style = getComputedStyle(element);
      return {
        x: rect.x, y: rect.y, width: rect.width, height: rect.height,
        font: style.font, color: style.color, background: style.background,
        border: style.border, radius: style.borderRadius, shadow: style.boxShadow
      };
    })
  ]));
  const common = [".bongocat-surface", ".side-nav", ".brand-logo-wrap", ".menu-item",
    ".content-header", ".page-scroll", ".section-title"];
  const pages = {};
  document.querySelector('[data-page="2"]').click();
  await delay(300);
  pages.model = snapshot([...common, ".model-grid", ".upload-card", ".model-card",
    ".model-preview", ".model-info", ".card-actions", ".card-action"]);
  document.querySelector('[data-action="open-behavior"]')?.click();
  await delay(100);
  pages.modal = snapshot([".modal-mask", ".modal", ".modal-head", ".modal-close",
    ".modal-body", ".segmented", ".segment", ".behavior-list",
    ".behavior-row", ".shortcut-editor", ".behavior-play-button"]);
  document.querySelector('[data-page="4"]').click();
  await delay(300);
  pages.support = snapshot([...common, ".support-page", ".support-hero",
    ".support-hero-logo-wrap", ".github-star", ".support-hero h2", ".support-byline",
    ".support-hero p", ".support-hero-footer", ".support-version", ".btn-primary",
    ".feedback-link", ".support-section", ".support-section-title",
    ".support-section-subtitle", ".support-icon-list", ".support-icon-link",
    ".support-app-icon"]);
  return pages;
})()`;

const result = await new Promise((resolve, reject) => {
  const timeout = setTimeout(() => reject(new Error("Browser probe timed out")), 10000);
  socket.addEventListener("open", () => socket.send(JSON.stringify({
    id: 1,
    method: "Runtime.evaluate",
    params: { expression, awaitPromise: true, returnByValue: true }
  })));
  socket.addEventListener("message", event => {
    const message = JSON.parse(event.data);
    if (message.id !== 1) return;
    clearTimeout(timeout);
    if (message.error) reject(new Error(JSON.stringify(message.error)));
    else if (message.result.exceptionDetails) reject(new Error(
      JSON.stringify(message.result.exceptionDetails)));
    else resolve(message.result.result.value);
    socket.close();
  });
  socket.addEventListener("error", reject);
});

process.stdout.write(`${JSON.stringify(result, null, 2)}\n`);
const browserSocket = new WebSocket(version.webSocketDebuggerUrl);
await new Promise(resolve => {
  browserSocket.addEventListener("open", () => browserSocket.send(JSON.stringify({
    id: 2, method: "Browser.close"
  })));
  browserSocket.addEventListener("close", resolve);
  browserSocket.addEventListener("error", resolve);
});
