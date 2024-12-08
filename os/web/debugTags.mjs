/** @type {Record<string, string>} */
const debugTags = {};

/** @type {any} */ (window).debugTags = debugTags;

/**
 *
 * @param {string} tag
 * @param {string | number | boolean} value
 */
export function addDebugTag(tag, value) {
    debugTags[tag] = `${value}`;
}
