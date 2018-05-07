declare const YT: any;

document.addEventListener('DOMContentLoaded', () => {
    console.log('DOMContentLoaded');
});

var tag = document.createElement('script');
tag.src = "https://www.youtube.com/iframe_api";
var firstScriptTag = document.getElementsByTagName('script')[0];
firstScriptTag.parentNode.insertBefore(tag, firstScriptTag);

var player: any;

// 全然わからない。俺たちは雰囲気でTypeScriptをやっている
(window as any).onYouTubeIframeAPIReady = () => {
//export function onYouTubeIframeAPIReady() {
    player = new YT.Player('player', {
        height: '360',
        width: '640',
        videoId: 'M7lc1UVf-VE',
        playerVars: {
            autoplay: 1,
            //showinfo: 0, 
            //controls: 0, 
            //modestbranding: 1, 
            //rel: 0, 
        }, 
        events: {
            //'onReady': onPlayerReady,
            //'onStateChange': onPlayerStateChange
        }
    });
}