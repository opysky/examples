declare const YT: any;

document.addEventListener('DOMContentLoaded', () => {
    console.log('DOMContentLoaded');
});

var tag = document.createElement('script');
tag.src = "https://www.youtube.com/iframe_api";
var firstScriptTag = document.getElementsByTagName('script')[0];
firstScriptTag.parentNode.insertBefore(tag, firstScriptTag);

let player: any;
const videoid = 'M7lc1UVf-VE';
//const videoid = 'eggR4dKQR_w'; // 4K

// 全然わからない。俺たちは雰囲気でTypeScriptをやっている
(window as any).onYouTubeIframeAPIReady = () => {
    player = new YT.Player('player', {
        height: '480',
        width: '640',
        //videoId: videoid,
        playerVars: {
            autoplay: 1,
            loop: 1,
            //playlist: videoid,
            //showinfo: 0, 
            //controls: 0, 
            //modestbranding: 1, 
            //rel: 0, 
            //iv_load_policy: 3, 
        },
        events: {
            'onReady': () => {
                const rates: Array<number> = player.getAvailablePlaybackRates();
                player.setPlaybackRate(rates[rates.length-1]);
            },
            //'onStateChange': onPlayerStateChange
        }
    });
};