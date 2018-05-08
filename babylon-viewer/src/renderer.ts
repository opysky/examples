import * as babylon from 'babylonjs';

export default class Renderer {
    private _canvas: HTMLCanvasElement;
    private _engine: babylon.Engine;
    private _scene: babylon.Scene;

    createScene(canvas: HTMLCanvasElement, engine: babylon.Engine) {
        this._canvas = canvas;
        this._engine = engine;
        const scene = new babylon.Scene(engine);
        this._scene = scene;

        const camera = new babylon.FreeCamera('camera1', new babylon.Vector3(0, 5, -10), scene);
        camera.setTarget(babylon.Vector3.Zero());
        camera.attachControl(canvas, true);

        const light = new babylon.HemisphericLight('light1', new babylon.Vector3(0, 1, 0), scene);
        light.intensity = 0.7;

        const sphere = babylon.Mesh.CreateSphere('sphere1', 16, 2, scene);
        sphere.position.y = 1;

        const ground = babylon.Mesh.CreateGround('ground1', 6, 6, 2, scene);
    }

    initialize(canvas: HTMLCanvasElement) {
        const engine = new babylon.Engine(canvas, true);
        this.createScene(canvas, engine);

        engine.runRenderLoop(() => {
            this._scene.render();
        });

        window.addEventListener('resize', function () {
            engine.resize();
        });
    }
}

document.addEventListener('DOMContentLoaded', () => {
    if (babylon.Engine.isSupported()) {
        const renderer = new Renderer();
        renderer.initialize(document.getElementById('render-canvas') as HTMLCanvasElement);
    }
});