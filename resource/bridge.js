/**
 * WebView2原生桥接器
 * 用于在Web应用和原生应用之间进行通信
 */
(() => {
    // 存储事件监听器的Map，key为事件名，value为监听器函数的Set
    const listeners = new Map();

    // 存储待处理请求的Map，key为请求ID，value为包含resolve、reject和timer的对象
    const pending = new Map();

    /**
     * 生成随机UUID
     * @returns {string} 生成的UUID字符串
     */
    function randomUUID() {
        // 优先使用crypto.randomUUID() API（如果可用）
        if (typeof crypto !== "undefined" && crypto.randomUUID) {
            return crypto.randomUUID();
        }

        // 备用方案：使用URL.createObjectURL生成唯一标识符
        const temp = URL.createObjectURL(new Blob());
        const uuid = temp.toString();
        URL.revokeObjectURL(temp);
        return uuid.substring(uuid.lastIndexOf("/") + 1);
    }

    /**
     * 处理从原生端接收到的消息
     * @param {MessageEvent} event - 消息事件对象
     */
    function onMessage(event) {
        const msg = event.data;
        if (!msg) {
            return;
        }

        // 处理响应消息（包含id和result字段）
        if (msg.id && msg.result !== undefined) {
            const p = pending.get(msg.id);
            if (p) {
                clearTimeout(p.timer);
                pending.delete(msg.id);

                // 检查是否有错误代码（非10000-19999范围的代码视为错误）
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

        // 处理事件消息（包含event字段）
        if (msg.event) {
            const set = listeners.get(msg.event);
            if (set) {
                // 遍历所有监听器并调用它们
                for (const fn of set) {
                    try {
                        fn(msg.data);
                    } catch (_) {
                        // 忽略监听器执行过程中的错误
                    }
                }
            }
        }
    }

    /**
     * 调用原生方法
     * @param {string} cmd - 要执行的命令名称
     * @param {any} [args] - 命令参数
     * @param {Object} [opts] - 选项配置
     * @param {number} [opts.timeout=3000] - 超时时间（毫秒）
     * @returns {Promise<any>} 返回Promise，解析为原生方法的执行结果
     */
    function invoke(cmd, args, opts) {
        const id = randomUUID();
        const payload = { id, cmd, args };

        // 向原生端发送消息
        window.chrome.webview.postMessage(payload);

        return new Promise((resolve, reject) => {
            const timeout = (opts && opts.timeout) || 3000;
            const timer = setTimeout(() => {
                pending.delete(id);
                reject(new Error("invoke 操作超时: " + cmd));
            }, timeout);

            // 存储Promise的resolve和reject函数，以便在收到响应时调用
            pending.set(id, { resolve, reject, timer });
        });
    }

    /**
     * 注册事件监听器
     * @param {string} event - 事件名称
     * @param {Function} fn - 事件处理函数
     * @returns {Function} 返回一个取消监听的函数
     */
    function on(event, fn) {
        if (!listeners.has(event)) {
            listeners.set(event, new Set());
        }
        listeners.get(event).add(fn);

        // 返回取消监听的函数
        return () => listeners.get(event)?.delete(fn);
    }

    // 将Native对象暴露到全局，提供invoke和on方法
    window.Native = { invoke, on };

    // 添加消息监听器，处理从原生端发送的消息
    window.chrome.webview.addEventListener("message", onMessage);
})();
