// Runs the C++ engine (compiled to WebAssembly) off the page's main thread,
// so the board stays responsive while the AI thinks.
importScripts('engine.js');

const ready = (async () => {
  const engine = await createEngine();
  const text = await (await fetch('enable1.txt')).text();
  const words = engine.init(text);
  return { engine, words };
})();

self.onmessage = async ({ data: { id, fn, args } }) => {
  try {
    const { engine, words } = await ready;
    const result = fn === 'ready' ? words : engine[fn](...args);
    self.postMessage({ id, result });
  } catch (err) {
    self.postMessage({ id, error: String(err && err.message ? err.message : err) });
  }
};
