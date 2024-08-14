import readline from "readline";

const { stdin, stdout } = process;

let width = stdout.columns;
let height = stdout.rows;

const write = (str) => stdout.write(str);
const esc = (str) => write("\x1b[" + str);

const init = () => {
    esc("48;5;0m"); // bg black
    esc("2J"); // clear screen
    esc("?25l"); // hide cursor
};

const exit = () => {
    esc("?25h"); // show cursor
    esc("37m"); // fg to white
    esc("48;5;0m"); // bg to black
    console.log("\nbye.");
    process.exit();
};

process.on("SIGINT", exit);
process.on("SIGWINCH", () => {
    init();
    width = stdout.columns;
    height = stdout.rows;
});

readline.emitKeypressEvents(stdin);
if (stdin.isTTY) {
    stdin.setRawMode(true);
}

let dx = 0;
let dy = 0;

stdin.on("keypress", (chunk, key) => {
    if (!key) return;
    if (key.name == "escape" || key.name == "q") {
        exit();
    }
    dx = 0;
    dy = 0;
    if (key.name == "a" || key.name == "left") dx = -1;
    if (key.name == "d" || key.name == "right") dx = 1;
    if (key.name == "w" || key.name == "up") dy = -1;
    if (key.name == "s" || key.name == "down") dy = 1;
});

// ======================

init();

let x = (width / 2) | 0;
let y = (height / 2) | 0;
let t = 0;

setInterval(() => {
    // Update
    t += 0.5;

    x += dx;
    if (x < 0) x = width;
    if (x > width) x = 0;

    y += dy;
    if (y < 0) y = height;
    if (y > height) y = 0;

    // Render
    const ch = " ";
    const col = (t | 0) % 255;

    esc(`${y};${x}H`); // cursor pos
    esc("38;5;0m"); // fg
    esc(`48;5;${col}m`); // bg
    write(ch);
}, 1000 / 30);
