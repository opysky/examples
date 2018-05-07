import {app, BrowserWindow} from 'electron';

let win: BrowserWindow;

app.on('ready', () => {
    win = new BrowserWindow({width: 800, height: 600});
    win.loadURL(`file://${__dirname}/../index.html`);
    win.webContents.openDevTools();
    win.on('closed', () => {
        win = null;
    });
});

app.on('window-all-closed', () => {
    if (process.platform !== 'darwin') {
        app.quit();
    }
});