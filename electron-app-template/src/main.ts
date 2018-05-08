import {app, BrowserWindow, Menu} from 'electron';

let mainWindow: BrowserWindow;

app.on('ready', createWindow);

app.on('window-all-closed', () => {
    if (process.platform !== 'darwin') {
        app.quit();
    }
});

app.on('activate', () => {
    if (mainWindow === null) {
        createWindow();
    }
});

function createWindow(): void {
    mainWindow = new BrowserWindow({width: 800, height: 600});

    mainWindow.loadURL(`file://${__dirname}/../index.html`);
    
    mainWindow.on('closed', () => {
        mainWindow = null;
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
                {label: 'Refresh(&R)', accelerator: 'CmdOrCtrl+R', click: () => mainWindow.webContents.reload()}
            ]
        }, 
        {
            label: 'Tools(&T)', 
            submenu: [
                {label: 'Toggle Developer Tools', accelerator: 'F12', click: () => mainWindow.webContents.toggleDevTools()}
            ]
        }
    ];
    Menu.setApplicationMenu(Menu.buildFromTemplate(menuTemplate));
}