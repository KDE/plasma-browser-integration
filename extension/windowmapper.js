/*
    Copyright (C) 2017 Kai Uwe Broulik <kde@privat.broulik.de>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
 */

document.title = "Plasma Browser Integration @PLASMABROWSERINTEGRATION"
               + Number(window.location.hash.substr(1)) + "@";

// if it failed to map the window within 2 seconds, something has horribly gone wrong
// just sneak out of the house before someone notices us!
window.setTimeout(function() {
    window.close();
}, 2000);
