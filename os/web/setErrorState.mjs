/**
 * @param {Error} err
 */
export function setErrorState(err) {
    // @ts-ignore
    document.getElementById("error_text").innerText =
        `\n\n${err.message}\n${err.stack}`;
}
