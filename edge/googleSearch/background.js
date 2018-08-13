browser.contextMenus.create({
    id: 'googleSearch', 
    title: 'Googleで「%s」を検索', 
    contexts: ['selection'], 
    onclick: (info, tab) => {
        browser.tabs.create({url: 'https://www.google.com/search?q=' + info.selectionText});
    }
});

browser.contextMenus.create({
    id: 'googleImageSearch', 
    title: 'Googleでこの画像を検索', 
    contexts: ['image'], 
    onclick: (info, tab) => {
        browser.tabs.create({url: 'http://www.google.co.jp/searchbyimage?image_url=' + info.srcUrl});
    }
});