/*
    Copyright (C) 2019 Kai Uwe Broulik <kde@privat.broulik.de>

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 3 of
    the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

(function() {

    // This is an Object so we can count the number of items ofr any given type
    // If any type comes up suspiciously often, we assume it's an overview page
    // and of no practical use.
    let foundTypes = {};

    const schemaOrgTags = document.querySelectorAll("[itemtype*='schema.org']");

    schemaOrgTags.forEach((tag) => {
        let type = tag.getAttribute("itemtype");
        // Using https for an URI is wrong but some websites do that...
        type = type.replace(/^https?:\/\/s?schema.org\//, "");
        if (!type) {
            return; // continue
        }

        foundTypes[type] = (foundTypes[type] || 0) + 1;
    });

    const jsonLdScriptTags = document.querySelectorAll("script[type='application/ld+json']");
    jsonLdScriptTags.forEach((tag) => {
        try {
            let json = JSON.parse(tag.innerText.replace(/(\r\n|\n|\r)/gm, ""));
            if (json) {
                if (!Array.isArray(json)) { // turn it into an array so we can use same codepath below
                    json = [json];
                }

                json.forEach((item) => {
                    let types = item["@type"];
                    if (!types) {
                        return; // continue
                    }

                    if (!Array.isArray(types)) {
                        types = [types];
                    }

                    types.forEach((type) => {
                        foundTypes[type] = (foundTypes[type] || 0) + 1;
                    });
                });
            }
        } catch (e) {
            console.warn("Failed to parse json ld in", tag, e);
            return;
        }
    });

    return foundTypes;

})();
