import readline from "readline";

let width = process.stdout.columns;
let height = process.stdout.rows;
let dx = 0;
let dy = 0;

const exit = () => {
    console.log("\x1b[38;5;1m\x1b[48;5;0m"); // reset fg/bg
    console.log("\x1b[?25h"); // show cursor
    process.exit();
};

process.on("SIGWINCH", () => {
    console.log("\x1b[38;5;1m\x1b[48;5;0m"); // reset fg/bg
    console.log("\x1b[2J"); // clear screen
    width = process.stdout.columns;
    height = process.stdout.rows;
});

readline.emitKeypressEvents(process.stdin);
if (process.stdin.isTTY) {
    process.stdin.setRawMode(true);
}

process.stdin.on("keypress", (chunk, key) => {
    if (!key) {
        return;
    }
    if (key.name == "escape" || key.name == "q") {
        exit();
    }
    if (key.name == "a" || key.name == "left") dx = -1;
    if (key.name == "d" || key.name == "right") dx = 1;
    if (key.name == "w" || key.name == "up") dy = -1;
    if (key.name == "s" || key.name == "down") dy = 1;
});

process.on("SIGINT", () => {
    exit();
});

console.log("\x1b[2J"); // clear screen
console.log("\x1b[?25l"); // hide cursor

let x = (width / 2) | 0;
let y = (height / 2) | 0;
let t = 0;
setInterval(() => {
    x += dx;
    if (x < 0) x = width;
    if (x > width) x = 0;

    y += dy;
    if (y < 0) y = height - 1;
    if (y >= height) y = 0;

    const ch = " "; // Math.random() < 0.5 ? "\\" : "/";
    const col = t % 255;
    dx = 0;
    dy = 0;
    console.log(`\x1b[${y};${x}H\x1b[38;5;0m\x1b[48;5;${col}m${ch}`);
    t++;
}, 10);
