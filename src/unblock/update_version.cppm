export module unblock.update_version;

import std;

/** Version of the latest release, taken from the URL the check request ended on:
 *  GitHub redirects /releases/latest to /releases/tag/v1.8.0. */
export std::optional<std::string> parseUpdateVersionFromUrl(std::string_view url);

/** Fallback when the request is served without a redirect: the release page
 *  links its tag as /<owner>/<repo>/tree/v<version>". Only the href is

 * * parsed,
 *  not the attributes around it — the markup the old parser relied on is gone
 *  from the release page and took the update check
 * with

 * * it.
 */
export std::optional<std::string> parseUpdateVersionFromHtml(std::string_view line);
