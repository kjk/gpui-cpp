// Slim user32/gdi32 helpers for driving native windows from bun tests.
// Inspired by SumatraPDF tests/winapi.ts: post messages (SendInput is dropped
// on this machine), PrintWindow captures work for background windows.

import { dlopen, FFIType, JSCallback, ptr } from "bun:ffi";

const user32 = dlopen("user32.dll", {
  EnumWindows: { args: [FFIType.function, FFIType.i64], returns: FFIType.bool },
  GetClassNameW: { args: [FFIType.ptr, FFIType.ptr, FFIType.i32], returns: FFIType.i32 },
  GetWindowThreadProcessId: { args: [FFIType.ptr, FFIType.ptr], returns: FFIType.u32 },
  PostMessageW: { args: [FFIType.ptr, FFIType.u32, FFIType.i64, FFIType.i64], returns: FFIType.bool },
  SendMessageW: { args: [FFIType.ptr, FFIType.u32, FFIType.i64, FFIType.i64], returns: FFIType.i64 },
  MoveWindow: {
    args: [FFIType.ptr, FFIType.i32, FFIType.i32, FFIType.i32, FFIType.i32, FFIType.bool],
    returns: FFIType.bool,
  },
  GetClientRect: { args: [FFIType.ptr, FFIType.ptr], returns: FFIType.bool },
  GetWindowRect: { args: [FFIType.ptr, FFIType.ptr], returns: FFIType.bool },
  GetWindowTextW: { args: [FFIType.ptr, FFIType.ptr, FFIType.i32], returns: FFIType.i32 },
  IsWindowVisible: { args: [FFIType.ptr], returns: FFIType.bool },
  ShowWindow: { args: [FFIType.ptr, FFIType.i32], returns: FFIType.bool },
  SetForegroundWindow: { args: [FFIType.ptr], returns: FFIType.bool },
  GetWindowDC: { args: [FFIType.ptr], returns: FFIType.u64 },
  ReleaseDC: { args: [FFIType.ptr, FFIType.u64], returns: FFIType.i32 },
  PrintWindow: { args: [FFIType.ptr, FFIType.u64, FFIType.u32], returns: FFIType.bool },
  GetSystemMetrics: { args: [FFIType.i32], returns: FFIType.i32 },
  SystemParametersInfoW: { args: [FFIType.u32, FFIType.u32, FFIType.ptr, FFIType.u32], returns: FFIType.bool },
  SetProcessDpiAwarenessContext: { args: [FFIType.i64], returns: FFIType.bool },
  SetCursorPos: { args: [FFIType.i32, FFIType.i32], returns: FFIType.bool },
  ClientToScreen: { args: [FFIType.ptr, FFIType.ptr], returns: FFIType.bool },
});

const gdi32 = dlopen("gdi32.dll", {
  CreateCompatibleDC: { args: [FFIType.u64], returns: FFIType.u64 },
  CreateCompatibleBitmap: { args: [FFIType.u64, FFIType.i32, FFIType.i32], returns: FFIType.u64 },
  SelectObject: { args: [FFIType.u64, FFIType.u64], returns: FFIType.u64 },
  DeleteObject: { args: [FFIType.u64], returns: FFIType.bool },
  DeleteDC: { args: [FFIType.u64], returns: FFIType.bool },
});

const gdiplus = dlopen("gdiplus.dll", {
  GdiplusStartup: { args: [FFIType.ptr, FFIType.ptr, FFIType.ptr], returns: FFIType.u32 },
  GdipCreateBitmapFromHBITMAP: { args: [FFIType.u64, FFIType.u64, FFIType.ptr], returns: FFIType.u32 },
  GdipSaveImageToFile: { args: [FFIType.u64, FFIType.ptr, FFIType.ptr, FFIType.ptr], returns: FFIType.u32 },
  GdipDisposeImage: { args: [FFIType.u64], returns: FFIType.u32 },
});

export const SW_RESTORE = 9;
export const WM_CLOSE = 0x0010;
export const WM_MOUSEMOVE = 0x0200;
export const WM_LBUTTONDOWN = 0x0201;
export const WM_LBUTTONUP = 0x0202;
export const MK_LBUTTON = 0x0001;
export const PW_RENDERFULLCONTENT = 0x00000002;

export type Rect = { left: number; top: number; right: number; bottom: number };

export function sleep(ms: number): Promise<void> {
  return Bun.sleep(ms);
}

export function packCoords(x: number, y: number): number {
  return ((y & 0xffff) << 16) | (x & 0xffff);
}

export function wideZ(s: string): Uint16Array {
  const buf = new Uint16Array(s.length + 1);
  for (let i = 0; i < s.length; i++) {
    buf[i] = s.charCodeAt(i);
  }
  buf[s.length] = 0;
  return buf;
}

export function setProcessDpiAware(): boolean {
  return user32.symbols.SetProcessDpiAwarenessContext(-4n as unknown as number);
}

export function getSystemMetrics(index: number): number {
  return user32.symbols.GetSystemMetrics(index);
}

export function getWorkArea(): Rect {
  const buf = new Int32Array(4);
  if (!user32.symbols.SystemParametersInfoW(0x0030, 0, ptr(buf), 0)) {
    return { left: 0, top: 0, right: getSystemMetrics(0), bottom: getSystemMetrics(1) };
  }
  return { left: buf[0]!, top: buf[1]!, right: buf[2]!, bottom: buf[3]! };
}

export function enumWindows(visit: (hwnd: number) => boolean): void {
  const cb = new JSCallback((hwnd: number) => visit(hwnd), {
    args: [FFIType.ptr, FFIType.i64],
    returns: FFIType.bool,
  });
  try {
    user32.symbols.EnumWindows(cb, 0n);
  } finally {
    cb.close();
  }
}

export function getClassName(hwnd: number): string {
  const buf = new Uint16Array(256);
  const n = user32.symbols.GetClassNameW(hwnd, ptr(buf), 256);
  let s = "";
  for (let i = 0; i < n; i++) {
    s += String.fromCharCode(buf[i]!);
  }
  return s;
}

export function getWindowPid(hwnd: number): number {
  const out = new Uint32Array(1);
  user32.symbols.GetWindowThreadProcessId(hwnd, ptr(out));
  return out[0]!;
}

export function getWindowText(hwnd: number): string {
  const buf = new Uint16Array(512);
  const n = user32.symbols.GetWindowTextW(hwnd, ptr(buf), 512);
  let s = "";
  for (let i = 0; i < n; i++) {
    s += String.fromCharCode(buf[i]!);
  }
  return s;
}

export function isWindowVisible(hwnd: number): boolean {
  return user32.symbols.IsWindowVisible(hwnd);
}

export function getWindowRect(hwnd: number): Rect {
  const buf = new Int32Array(4);
  user32.symbols.GetWindowRect(hwnd, ptr(buf));
  return { left: buf[0]!, top: buf[1]!, right: buf[2]!, bottom: buf[3]! };
}

export function getClientRect(hwnd: number): Rect {
  const buf = new Int32Array(4);
  user32.symbols.GetClientRect(hwnd, ptr(buf));
  return { left: buf[0]!, top: buf[1]!, right: buf[2]!, bottom: buf[3]! };
}

export function moveWindow(hwnd: number, x: number, y: number, w: number, h: number): boolean {
  return user32.symbols.MoveWindow(hwnd, x, y, w, h, true);
}

export function showWindow(hwnd: number, cmd: number): boolean {
  return user32.symbols.ShowWindow(hwnd, cmd);
}

export type WorkAreaHalf = "left" | "right";

// Place on the left or right half of the primary work area (taskbar excluded).
// Height is 80% of the work area; the window stays top-aligned.
export function placeOnWorkAreaHalf(hwnd: number, side: WorkAreaHalf): boolean {
  if (!hwnd) {
    return false;
  }
  showWindow(hwnd, SW_RESTORE);
  const wa = getWorkArea();
  const mid = wa.left + Math.floor((wa.right - wa.left) / 2);
  const x = side === "left" ? wa.left : mid;
  const w = side === "left" ? mid - wa.left : wa.right - mid;
  const h = Math.floor((wa.bottom - wa.top) * 0.8);
  return moveWindow(hwnd, x, wa.top, w, h);
}

export function setForegroundWindow(hwnd: number): boolean {
  return user32.symbols.SetForegroundWindow(hwnd);
}

export function setCursorPos(x: number, y: number): boolean {
  return user32.symbols.SetCursorPos(x, y);
}

export function clientToScreen(hwnd: number, x: number, y: number): { x: number; y: number } {
  const buf = new Int32Array([x, y]);
  user32.symbols.ClientToScreen(hwnd, ptr(buf));
  return { x: buf[0]!, y: buf[1]! };
}

export function sendMessage(hwnd: number, msg: number, wParam: number, lParam: number): bigint {
  return user32.symbols.SendMessageW(hwnd, msg, BigInt(wParam), BigInt(lParam));
}

export function postMessage(hwnd: number, msg: number, wParam: number, lParam: number): boolean {
  return user32.symbols.PostMessageW(hwnd, msg, BigInt(wParam), BigInt(lParam));
}

export function findVisiblePidWindow(pid: number): number {
  let found = 0;
  enumWindows((hwnd) => {
    if (getWindowPid(hwnd) !== pid || !isWindowVisible(hwnd)) {
      return true;
    }
    const r = getWindowRect(hwnd);
    if (r.right - r.left < 80 || r.bottom - r.top < 80) {
      return true;
    }
    found = hwnd;
    return false;
  });
  return found;
}

export async function waitForPidWindow(pid: number, timeoutMs = 20000): Promise<number> {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    const h = findVisiblePidWindow(pid);
    if (h) {
      return h;
    }
    await sleep(150);
  }
  return 0;
}

function isSizedWindow(hwnd: number): boolean {
  if (!isWindowVisible(hwnd)) {
    return false;
  }
  const r = getWindowRect(hwnd);
  return r.right - r.left >= 80 && r.bottom - r.top >= 80;
}

export function findVisibleClassWindows(className: string): number[] {
  const found: number[] = [];
  enumWindows((hwnd) => {
    if (!isSizedWindow(hwnd) || getClassName(hwnd) !== className) {
      return true;
    }
    found.push(hwnd);
    return true;
  });
  return found;
}

// Wait for a visible window of `className` that is not in `ignore` (hwnds
// that already existed before launch). Needed when the app is a child of
// WinDbg and does not share the spawned PID.
export async function waitForNewClassWindow(
  className: string,
  ignore: Set<number>,
  timeoutMs = 60000,
): Promise<number> {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    for (const h of findVisibleClassWindows(className)) {
      if (!ignore.has(h)) {
        return h;
      }
    }
    await sleep(200);
  }
  return 0;
}

export async function clickClient(hwnd: number, x: number, y: number, settleMs = 200): Promise<void> {
  const lp = packCoords(x, y);
  sendMessage(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, lp);
  sendMessage(hwnd, WM_LBUTTONUP, 0, lp);
  if (settleMs) {
    await sleep(settleMs);
  }
}

export async function hoverClient(hwnd: number, x: number, y: number, settleMs = 200): Promise<void> {
  const lp = packCoords(x, y);
  const scr = clientToScreen(hwnd, x, y);
  setCursorPos(scr.x, scr.y);
  sendMessage(hwnd, WM_MOUSEMOVE, 0, lp);
  if (settleMs) {
    await sleep(settleMs);
  }
}

const PNG_ENCODER_CLSID = new Uint8Array([
  0x06, 0xf4, 0x7c, 0x55, 0x04, 0x1a, 0xd3, 0x11, 0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e,
]);

let gdiplusStarted = false;
function ensureGdiplus(): void {
  if (gdiplusStarted) {
    return;
  }
  const input = new Uint8Array(24);
  new DataView(input.buffer).setUint32(0, 1, true);
  const token = new BigUint64Array(1);
  if (gdiplus.symbols.GdiplusStartup(ptr(token), ptr(input), 0) === 0) {
    gdiplusStarted = true;
  }
}

export function captureWindowToPng(hwnd: number, outPath: string): boolean {
  ensureGdiplus();
  const r = getWindowRect(hwnd);
  const w = r.right - r.left;
  const h = r.bottom - r.top;
  if (w <= 0 || h <= 0) {
    return false;
  }
  const winDC = user32.symbols.GetWindowDC(hwnd);
  const memDC = gdi32.symbols.CreateCompatibleDC(winDC);
  const bmp = gdi32.symbols.CreateCompatibleBitmap(winDC, w, h);
  const oldObj = gdi32.symbols.SelectObject(memDC, bmp);
  user32.symbols.PrintWindow(hwnd, memDC, PW_RENDERFULLCONTENT);
  gdi32.symbols.SelectObject(memDC, oldObj);

  const gpBmp = new BigUint64Array(1);
  gdiplus.symbols.GdipCreateBitmapFromHBITMAP(bmp, 0n, ptr(gpBmp));
  const status = gdiplus.symbols.GdipSaveImageToFile(gpBmp[0]!, ptr(wideZ(outPath)), ptr(PNG_ENCODER_CLSID), 0);
  gdiplus.symbols.GdipDisposeImage(gpBmp[0]!);

  gdi32.symbols.DeleteObject(bmp);
  gdi32.symbols.DeleteDC(memDC);
  user32.symbols.ReleaseDC(hwnd, winDC);
  return status === 0;
}

export async function killAndWait(proc: Bun.Subprocess | undefined | null, timeoutMs = 5000): Promise<void> {
  if (!proc) {
    return;
  }
  try {
    proc.kill();
  } catch {
    /* already gone */
  }
  const deadline = Date.now() + timeoutMs;
  while (proc.exitCode === null && Date.now() < deadline) {
    await sleep(40);
  }
}
