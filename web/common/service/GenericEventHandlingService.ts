import { PalmButton } from '@common/bridge/Cloudpilot';
import { hardwareKeysFromCode } from '@common/helper/hardwareKeyboard';
import { AbstractCanvasDisplayService } from '@common/service/AbstractCanvasDisplayService';
import { AbstractEmulationService } from '@common/service/AbstractEmulationService';

const enum Area {
    screen,
    silkscreen,
    buttons,
    keyboard,
}

interface InteractionScreen {
    area: Area.screen | Area.silkscreen;
}

interface InteractionButton {
    area: Area.buttons;
    buttonPrimary: PalmButton;
    buttonSecondary?: PalmButton;
}

interface InteractionKeyboard {
    area: Area.keyboard;
    // Keys held by this interaction (latched modifiers first), released in reverse order
    keys: Array<number>;
}

type Interaction = InteractionScreen | InteractionButton | InteractionKeyboard;

/**
 * DOM event handler callback.
 */
export type EventHandler<K extends keyof HTMLElementEventMap> = (ev: HTMLElementEventMap[K]) => void;

/**
 * A DOM event target.
 */
export interface EmulatorEventTarget {
    addEventListener<K extends keyof HTMLElementEventMap>(
        type: K,
        handler: EventHandler<K>,
        options?: boolean | AddEventListenerOptions,
    ): void;
    removeEventListener<K extends keyof HTMLElementEventMap>(
        type: K,
        listener: EventHandler<K>,
        options?: boolean | EventListenerOptions,
    ): void;
}

export class GenericEventHandlingService {
    constructor(
        protected emulationService: AbstractEmulationService,
        protected canvasDisplayService: AbstractCanvasDisplayService,
    ) {}

    bind(pointerEventTarget: EmulatorEventTarget, dpad: boolean, keyEventTarget: EmulatorEventTarget = window): void {
        this.dpad = dpad;

        if (this.pointerEventTarget) {
            this.release();
        }

        this.pointerEventTarget = pointerEventTarget;
        this.keyEventTarget = keyEventTarget || window;

        this.addEventListener(pointerEventTarget, 'mousedown', this.handleMouseDown);
        this.addEventListener(window, 'mouseup', this.handeMouseUp);
        this.addEventListener(window, 'mousemove', this.handleMouseMove);

        this.addEventListener(pointerEventTarget, 'touchstart', this.handleTouchStart);
        this.addEventListener(pointerEventTarget, 'touchmove', this.handleTouchMove);
        this.addEventListener(pointerEventTarget, 'touchend', this.handleTouchEnd);
        this.addEventListener(pointerEventTarget, 'touchcancel', this.handleTouchEnd);

        this.addEventListener(this.keyEventTarget, 'keydown', this.handleKeyDown);
        this.addEventListener(this.keyEventTarget, 'keyup', this.handleKeyUp);
        this.addEventListener(this.keyEventTarget, 'blur', this.releaseHardwareKeys);
    }

    release(): void {
        if (!this.pointerEventTarget) return;

        this.removeEventListener(this.pointerEventTarget, 'mousedown', this.handleMouseDown);
        this.removeEventListener(window, 'mouseup', this.handeMouseUp);
        this.removeEventListener(window, 'mousemove', this.handleMouseMove);

        this.removeEventListener(this.pointerEventTarget, 'touchstart', this.handleTouchStart);
        this.removeEventListener(this.pointerEventTarget, 'touchmove', this.handleTouchMove);
        this.removeEventListener(this.pointerEventTarget, 'touchend', this.handleTouchEnd);
        this.removeEventListener(this.pointerEventTarget, 'touchcancel', this.handleTouchEnd);

        this.removeEventListener(this.keyEventTarget, 'keydown', this.handleKeyDown);
        this.removeEventListener(this.keyEventTarget, 'keyup', this.handleKeyUp);
        this.removeEventListener(this.keyEventTarget, 'blur', this.releaseHardwareKeys);

        this.releaseHardwareKeys();

        for (const button of this.activeButtons) {
            this.handleButtonUp(button);
        }

        this.activeButtons.clear();

        for (const key of this.activeOnScreenKeys) {
            this.emulationService.handleHardwareKeyUp(key);
        }
        this.activeOnScreenKeys.clear();
        this.latchedOnScreenKeys.clear();

        this.interactionsTouch.clear();
        this.interactionMouse = undefined;

        this.pointerEventTarget = undefined;
    }

    isGameMode(): boolean {
        return this.gameMode;
    }

    setGameMode(enableGameMode: boolean): void {
        this.gameMode = enableGameMode;
        if (enableGameMode) this.releaseHardwareKeys();

        this.onToggleGameMode();
    }

    setGameModeHotkeyEnabled(enableGamemodeHotkey: boolean): void {
        this.enableGamemodeHotkey = enableGamemodeHotkey;
    }

    isGameModeHotkeyEnabled(): boolean {
        return this.enableGamemodeHotkey;
    }

    protected enableKeyboardEvents(): boolean {
        return true;
    }

    protected onToggleGameMode(): void {}

    protected addEventListener<K extends keyof HTMLElementEventMap, E extends EmulatorEventTarget>(
        elt: E,
        type: K,
        handler: EventHandler<K>,
        caputure?: boolean,
    ) {
        elt.addEventListener(type, handler, caputure);
    }

    protected removeEventListener<K extends keyof HTMLElementEventMap, E extends EmulatorEventTarget>(
        elt: E,
        type: K,
        handler: EventHandler<K>,
        caputure?: boolean,
    ) {
        elt.removeEventListener(type, handler, caputure);
    }

    private handleMouseDown = (e: MouseEvent): void => {
        if (e.button !== 0) return;

        const coords = this.canvasDisplayService.eventToPalmCoordinates(e);
        if (!coords) return;

        const area = this.determineArea(coords);

        if (area === Area.keyboard) {
            this.interactionMouse = { area, keys: this.handleOnScreenKeyDown(coords) };
        } else if (area === Area.buttons) {
            const button = this.canvasDisplayService.determineButton(coords);
            this.interactionMouse = { area, buttonPrimary: button };

            this.handleButtonDown(button);
        } else {
            this.interactionMouse = { area };
            this.emulationService.handlePointerMove(...coords, area === Area.silkscreen);
        }
    };

    private handleMouseMove = (e: MouseEvent): void => {
        // tslint:disable-next-line: no-bitwise
        if (
            !(e.buttons & 0x01) ||
            !this.interactionMouse ||
            this.interactionMouse?.area === Area.buttons ||
            this.interactionMouse?.area === Area.keyboard
        ) {
            return;
        }

        const coords = this.canvasDisplayService.eventToPalmCoordinates(e, true);
        if (!coords) return;

        this.emulationService.handlePointerMove(...coords, this.interactionMouse?.area === Area.silkscreen);
    };

    private handeMouseUp = (e: MouseEvent): void => {
        if (e.button !== 0) return;

        const interaction = this.interactionMouse;
        this.interactionMouse = undefined;

        switch (interaction?.area) {
            case Area.buttons:
                this.handleButtonUp(interaction.buttonPrimary);

                break;

            case Area.keyboard:
                this.handleOnScreenKeyUp(interaction.keys);

                break;

            case Area.screen:
            case Area.silkscreen:
                this.emulationService.handlePointerUp();

                break;
        }
    };

    private handleTouchStart = (e: TouchEvent): void => {
        let handled = false;

        for (let i = 0; i < e.changedTouches.length; i++) {
            const touch = e.changedTouches.item(i);
            if (!touch) continue;

            const coords = this.canvasDisplayService.eventToPalmCoordinates(touch);
            if (!coords) continue;

            handled = true;

            const area = this.determineArea(coords);
            if (area === Area.keyboard) {
                this.interactionsTouch.set(touch.identifier, { area, keys: this.handleOnScreenKeyDown(coords) });
            } else if (area === Area.buttons) {
                const button = this.canvasDisplayService.determineButton(coords);
                this.interactionsTouch.set(touch.identifier, { area, buttonPrimary: button });

                this.handleButtonDown(button);
            } else {
                this.interactionsTouch.set(touch.identifier, { area });
                this.emulationService.handlePointerMove(...coords, area === Area.silkscreen);
            }
        }

        if (handled && e.cancelable !== false) e.preventDefault();
    };

    private handleTouchMove = (e: TouchEvent): void => {
        let handled = false;

        for (let i = 0; i < e.changedTouches.length; i++) {
            const touch = e.changedTouches.item(i);
            if (!touch) continue;

            const interaction = this.interactionsTouch.get(touch.identifier);
            if (!interaction) continue;

            handled = true;

            const area = interaction.area;
            switch (area) {
                case Area.buttons: {
                    const coords = this.canvasDisplayService.eventToPalmCoordinates(touch);

                    if (!coords || this.determineArea(coords) !== Area.buttons) {
                        this.handleButtonUp(interaction.buttonSecondary);
                        interaction.buttonSecondary = undefined;

                        break;
                    }

                    const button = this.canvasDisplayService.determineButton(coords);

                    if (button === interaction.buttonPrimary) {
                        this.handleButtonUp(interaction.buttonSecondary);
                        interaction.buttonSecondary = undefined;
                    } else if (button !== interaction.buttonSecondary) {
                        this.handleButtonUp(interaction.buttonSecondary);
                        interaction.buttonSecondary = button;

                        this.handleButtonDown(button);
                    }

                    break;
                }

                case Area.screen:
                case Area.silkscreen: {
                    const coords = this.canvasDisplayService.eventToPalmCoordinates(touch, true);
                    if (!coords) continue;

                    this.emulationService.handlePointerMove(...coords, area === Area.silkscreen);

                    break;
                }

                default:
                    continue;
            }
        }

        if (handled && e.cancelable !== false) e.preventDefault();
    };

    private handleTouchEnd = (e: TouchEvent): void => {
        let handled = false;

        for (let i = 0; i < e.changedTouches.length; i++) {
            const touch = e.changedTouches.item(i);
            if (!touch) continue;

            const interaction = this.interactionsTouch.get(touch.identifier);
            if (interaction) handled = true;

            this.interactionsTouch.delete(touch.identifier);

            switch (interaction?.area) {
                case Area.buttons:
                    this.handleButtonUp(interaction.buttonPrimary);
                    this.handleButtonUp(interaction.buttonSecondary);

                    break;

                case Area.keyboard:
                    this.handleOnScreenKeyUp(interaction.keys);

                    break;

                case Area.screen:
                case Area.silkscreen:
                    this.emulationService.handlePointerUp();

                    break;
            }
        }

        if (handled && e.cancelable !== false) e.preventDefault();
    };

    private handleKeyDown = (e: KeyboardEvent): void => {
        if (this.emulationService.isSuspended() || !this.enableKeyboardEvents() || e.metaKey) return;

        if (this.isToggleGameMode(e)) {
            e.preventDefault();

            this.gameMode = !this.gameMode;
            if (this.gameMode) this.releaseHardwareKeys();
            this.onToggleGameMode();

            return;
        }

        if (this.usesHardwareKeyboard()) {
            // Unmapped keys are ignored; they must not fall through to the game mode buttons.
            const keys = hardwareKeysFromCode(e.code);
            if (keys === undefined) return;

            e.preventDefault();

            if (!this.activeHardwareKeys.has(e.code)) {
                this.activeHardwareKeys.set(e.code, keys);
                keys.forEach((key) => this.emulationService.handleHardwareKeyDown(key));
            }

            return;
        }

        const button = this.buttonFromEvent(e);
        if (button !== undefined) {
            this.handleButtonDown(button);
            e.preventDefault();

            return;
        }

        const keyCode = this.keyCodeFromEvent(e);
        if (keyCode !== undefined && !e.ctrlKey) {
            this.emulationService.handleKeyDown(keyCode);
            e.preventDefault();
        }
    };

    private handleKeyUp = (e: KeyboardEvent): void => {
        // Release a pressed device key even while a modal is open; otherwise it stays down.
        const hardwareKeys = this.activeHardwareKeys.get(e.code);
        if (hardwareKeys !== undefined) {
            this.activeHardwareKeys.delete(e.code);
            this.releaseDeviceKeys(hardwareKeys);
            e.preventDefault();

            return;
        }

        if (this.emulationService.isSuspended() || !this.enableKeyboardEvents()) return;

        const button = this.buttonFromEvent(e);
        if (button !== undefined) {
            this.handleButtonUp(button);
            e.preventDefault();

            return;
        }

        if (e.key === 'Control') {
            for (const btn of this.activeButtons) {
                this.emulationService.handleButtonUp(btn);
            }

            this.activeButtons.clear();
            void this.canvasDisplayService.drawButtons([]);
        }
    };

    private determineArea(coords: [number, number]): Area {
        if (this.canvasDisplayService.isKeyboard(coords)) {
            return Area.keyboard;
        }

        if (this.canvasDisplayService.isButtons(coords)) {
            return Area.buttons;
        }

        if (this.canvasDisplayService.isSilkscreen(coords)) {
            return Area.silkscreen;
        }

        return Area.screen;
    }

    private handleButtonDown(button: PalmButton): void {
        if (this.activeButtons.has(button)) return;

        this.activeButtons.add(button);
        this.emulationService.handleButtonDown(button);
        void this.canvasDisplayService.drawButtons(Array.from(this.activeButtons.values()));
    }

    private handleButtonUp(button: PalmButton | undefined): void {
        if (button === undefined || !this.activeButtons.has(button)) return;

        this.activeButtons.delete(button);
        this.emulationService.handleButtonUp(button);
        void this.canvasDisplayService.drawButtons(Array.from(this.activeButtons.values()));
    }

    // On-screen keyboard: a modifier tap toggles its latch; any other key is pressed together
    // with the latched modifiers, which are then cleared. Returns the keys now held.
    private handleOnScreenKeyDown(coords: [number, number]): Array<number> {
        const key = this.canvasDisplayService.determineKeyboardKey(coords);
        if (!key) return [];

        if (key.toggle) {
            this.handleOnScreenKeyboardToggle();

            return [];
        }

        if (key.modifier) {
            if (!this.latchedOnScreenKeys.delete(key.key)) this.latchedOnScreenKeys.add(key.key);
            this.drawOnScreenKeyboard();

            return [];
        }

        const keys = [...this.latchedOnScreenKeys, key.key];
        this.latchedOnScreenKeys.clear();

        for (const k of keys) {
            this.activeOnScreenKeys.add(k);
            this.emulationService.handleHardwareKeyDown(k);
        }

        this.drawOnScreenKeyboard();

        return keys;
    }

    private handleOnScreenKeyboardToggle(): void {
        const keys = [...this.activeOnScreenKeys];
        this.activeOnScreenKeys.clear();
        this.latchedOnScreenKeys.clear();
        this.releaseDeviceKeys(keys);

        void this.canvasDisplayService.toggleKeyboard();
    }

    private handleOnScreenKeyUp(keys: Array<number>): void {
        if (keys.length === 0) return;

        for (const k of keys) this.activeOnScreenKeys.delete(k);
        this.releaseDeviceKeys(keys);

        this.drawOnScreenKeyboard();
    }

    private drawOnScreenKeyboard(): void {
        this.canvasDisplayService.drawKeyboard(this.activeOnScreenKeys, this.latchedOnScreenKeys);
    }

    private releaseHardwareKeys = (): void => {
        const chords = [...this.activeHardwareKeys.values()];
        this.activeHardwareKeys.clear();

        for (const keys of chords) this.releaseDeviceKeys(keys);
    };

    // Releases device keys in reverse order, except keys still held by another host key or an
    // on-screen key (e.g. Blue held with F7 while a digit chord Blue + Q is released).
    private releaseDeviceKeys(keys: Array<number>): void {
        for (let i = keys.length - 1; i >= 0; i--) {
            if (!this.isDeviceKeyHeld(keys[i])) this.emulationService.handleHardwareKeyUp(keys[i]);
        }
    }

    private isDeviceKeyHeld(key: number): boolean {
        if (this.activeOnScreenKeys.has(key)) return true;

        for (const keys of this.activeHardwareKeys.values()) {
            if (keys.includes(key)) return true;
        }

        return false;
    }

    // Devices with a hardware keyboard get raw device keys unless game mode is on.
    private usesHardwareKeyboard(): boolean {
        return !this.gameMode && this.emulationService.supportsHardwareKeyboard();
    }

    private isToggleGameMode(e: KeyboardEvent): boolean {
        return (
            this.isGameModeHotkeyEnabled() && ((e.key === 'Shift' && e.ctrlKey) || (e.key === 'Control' && e.shiftKey))
        );
    }

    private isGameModeActive(e: KeyboardEvent): boolean {
        return (
            (!this.gameMode && e.ctrlKey) ||
            (this.gameMode && !e.ctrlKey) ||
            !this.emulationService.supportsKeyboardIO()
        );
    }

    private buttonFromEvent(e: KeyboardEvent): PalmButton | undefined {
        switch (e.key) {
            case 'PageUp':
                return PalmButton.up;

            case 'PageDown':
                return PalmButton.down;
        }

        if (!this.isGameModeActive(e)) return;

        switch (e.key) {
            case 'w':
            case 'i':
            case 'ArrowUp':
                return PalmButton.up;

            case 's':
            case 'k':
            case 'ArrowDown':
                return PalmButton.down;

            case 'a':
            case 'j':
            case 'y':
            case 'z':
                return PalmButton.cal;

            case 'ArrowLeft':
                return this.dpad ? PalmButton.rockerLeft : PalmButton.cal;

            case 'q':
            case 'u':
            case 'x':
                return PalmButton.phone;

            case 'e':
            case 'o':
            case 'c':
                return PalmButton.todo;

            case 'd':
            case 'l':
            case 'v':
                return PalmButton.notes;

            case 'ArrowRight':
                return this.dpad ? PalmButton.rockerRight : PalmButton.notes;
        }

        if (this.dpad) {
            switch (e.key) {
                case 'Enter':
                case ' ':
                    return PalmButton.rockerEnter;
            }
        }

        return undefined;
    }

    private keyCodeFromEvent(e: KeyboardEvent): number | undefined {
        if (this.isGameModeActive(e)) return;

        if (e.key.length === 1 && e.key.charCodeAt(0) <= 255) return e.key.charCodeAt(0);

        switch (e.key) {
            case 'ArrowLeft':
                return 0x1c;

            case 'ArrowRight':
                return 0x1d;

            case 'ArrowUp':
                return 0x1e;

            case 'ArrowDown':
                return 0x1f;

            case 'Backspace':
                return 0x08;

            case 'Tab':
                return 0x09;

            case 'Enter':
                return 0x0a;

            default:
                return undefined;
        }
    }

    protected dpad = false;

    private pointerEventTarget: EmulatorEventTarget | undefined;
    private keyEventTarget: EmulatorEventTarget = window;

    private interactionMouse: Interaction | undefined;
    private interactionsTouch = new Map<number, Interaction>();
    private activeButtons = new Set<PalmButton>();
    private activeHardwareKeys = new Map<string, Array<number>>();
    private activeOnScreenKeys = new Set<number>();
    private latchedOnScreenKeys = new Set<number>();
    private gameMode = false;
    private enableGamemodeHotkey = true;
}
