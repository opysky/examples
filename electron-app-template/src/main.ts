import {app, BrowserWindow, Menu} from 'electron';

let win: BrowserWindow;

app.on('ready', () => {
    win = new BrowserWindow({width: 800, height: 600});

    win.loadURL(`file://${__dirname}/../index.html`);
    
    win.on('closed', () => {
        win = null;
    });

    const menuTemplate: Electron.MenuItemConstructorOptions[] = [
        {
            label: 'File(&F)', 
            submenu: [
                {label: 'Exit(&X)', click: () => app.quit()}
            ]
        }, 
        {
            label: 'Edit(&E)', 
            submenu: [
                {label: 'Refresh(&R)', accelerator: 'CmdOrCtrl+R', click: () => win.webContents.reload()}
            ]
        }, 
        {
            label: 'Tools(&T)', 
            submenu: [
                {label: 'Toggle Developer Tools', accelerator: 'F12', click: () => win.webContents.toggleDevTools()}
            ]
        }
    ];
    Menu.setApplicationMenu(Menu.buildFromTemplate(menuTemplate));
});

app.on('window-all-closed', () => {
    if (process.platform !== 'darwin') {
        app.quit();
    }
});