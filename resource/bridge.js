(() => {
    const listeners = new Map();
    const pending = new Map();

    function randomUUID() {
        if (typeof crypto !== "undefined" && crypto.randomUUID) {
            return crypto.randomUUID();
        }

        const temp = URL.createObjectURL(new Blob());
        const uuid = temp.toString();
        URL.revokeObjectURL(temp);
        return uuid.substring(uuid.lastIndexOf("/") + 1);
    }

    function onMessage(event) {
        const msg = event.data;
        if (!msg) {
            return;
        }
        if (msg.id && msg.result !== undefined) {
            const p = pending.get(msg.id);
            if (p) {
                clearTimeout(p.timer);
                pending.delete(msg.id);
                if (
                    msg.result &&
                    msg.result.code &&
                    !(msg.result.code >= 10000 && msg.result.code < 20000)
                ) {
                    p.reject(new Error(msg.result.msg || "Native error!"));
                } else {
                    p.resolve(msg.result);
                }
            }
            return;
        }
        if (msg.event) {
            const set = listeners.get(msg.event);
            if (set) {
                for (const fn of set) {
                    try {
                        fn(msg.data);
                    } catch (_) {}
                }
            }
        }
    }

    function invoke(cmd, args, opts) {
        const id = randomUUID();
        const payload = { id, cmd, args };
        window.chrome.webview.postMessage(payload);
        return new Promise((resolve, reject) => {
            const timeout = (opts && opts.timeout) || 3000;
            const timer = setTimeout(() => {
                pending.delete(id);
                reject(new Error("invoke 操作超时: " + cmd));
            }, timeout);
            pending.set(id, { resolve, reject, timer });
        });
    }

    function on(event, fn) {
        if (!listeners.has(event)) {
            listeners.set(event, new Set());
        }
        listeners.get(event).add(fn);
        return () => listeners.get(event)?.delete(fn);
    }

    window.Native = { invoke, on };
    window.chrome.webview.addEventListener("message", onMessage);
})();
