/**
 * This function injects javascript file and waits until it is loaded
 * It allows JS files to set global variables when using ES modules
 *
 * @param {string} src
 */
export async function loadJs(src) {
    const el = document.createElement("script");
    el.src = src;
    document.body.appendChild(el);
    await new Promise((res, rej) => {
        el.onload = res;
        el.onerror = rej;
    });
}
