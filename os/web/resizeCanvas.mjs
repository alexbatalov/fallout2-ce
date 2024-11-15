export function resizeCanvas() {
    const canvas = /** @type {HTMLCanvasElement | null} */ (
        document.getElementById("canvas")
    );
    if (!canvas) {
        throw new Error(`Canvas element is not found`);
    }
    if (!canvas.parentElement) {
        throw new Error(`Canvas element have no parent`);
    }
    let parentWidth = canvas.parentElement.clientWidth;
    let parentHeight = canvas.parentElement.clientHeight;

    console.info(`resizeCanvas parent raw ${parentWidth}x${parentHeight}`);

    if (parentWidth / parentHeight > canvas.width / canvas.height) {
        parentWidth = (parentHeight * canvas.width) / canvas.height;
    } else {
        parentHeight = parentWidth / (canvas.width / canvas.height);
    }
    console.info(`resizeCanvas parent updated ${parentWidth}x${parentHeight}`);

    canvas.style.width = `${parentWidth}px`;
    canvas.style.height = `${parentHeight}px`;
}
