import { configuration } from "./config.mjs";

const GAMES_CACHE_PREFIX = "fscache";

const DELIMITER = "____";

/**
 *
 * @param {string} game
 * @param {string} filesVersion
 */
export function getCacheName(game, filesVersion) {
    return [GAMES_CACHE_PREFIX, game, filesVersion].join(DELIMITER);
}

/**
 *
 * @param {string} game
 * @param {string | null} keepFilesVersion
 */
export async function removeGameCache(game, keepFilesVersion) {
    const keepCache =
        keepFilesVersion !== null ? getCacheName(game, keepFilesVersion) : null;

    const prefix = [GAMES_CACHE_PREFIX, game, ""].join(DELIMITER);
    for (const key of await caches.keys()) {
        if (keepCache !== null && key === keepCache) {
            continue;
        }
        if (key.startsWith(prefix)) {
            console.info(`Cleaning cache ${key}`);
            await caches.delete(key);
        }
    }
}

export async function removeOldCache() {
    for (const game of configuration.games) {
        await removeGameCache(game.folder, game.filesVersion);
    }
}
