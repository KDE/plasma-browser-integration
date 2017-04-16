document.title = "Plasma Browser Integration @PLASMABROWSERINTEGRATION"
               + Number(window.location.hash.substr(1)) + "@";

// if it failed to map the window within 2 seconds, something has horribly gone wrong
// just sneak out of the house before someone notices us!
window.setTimeout(function() {
    window.close();
}, 2000);
