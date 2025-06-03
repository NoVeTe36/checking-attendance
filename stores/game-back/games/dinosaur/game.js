// JavaScript code for the dinosaur game
const canvas = document.getElementById('gameCanvas');
const ctx = canvas.getContext('2d');

let dinosaur = {
    x: 50,
    y: canvas.height - 50,
    width: 30,
    height: 30,
    gravity: 0.5,
    jumpPower: 10,
    velocityY: 0,
    isJumping: false
};

let obstacles = [];
let score = 0;
let gameOver = false;

function startGame() {
    document.addEventListener('keydown', handleKeyDown);
    requestAnimationFrame(gameLoop);
}

function handleKeyDown(event) {
    if (event.code === 'Space' && !dinosaur.isJumping) {
        dinosaur.isJumping = true;
        dinosaur.velocityY = -dinosaur.jumpPower;
    }
}

function gameLoop() {
    if (gameOver) return;

    ctx.clearRect(0, 0, canvas.width, canvas.height);
    updateDinosaur();
    updateObstacles();
    drawDinosaur();
    drawObstacles();
    checkCollisions();
    updateScore();

    requestAnimationFrame(gameLoop);
}

function updateDinosaur() {
    dinosaur.velocityY += dinosaur.gravity;
    dinosaur.y += dinosaur.velocityY;

    if (dinosaur.y + dinosaur.height >= canvas.height) {
        dinosaur.y = canvas.height - dinosaur.height;
        dinosaur.isJumping = false;
        dinosaur.velocityY = 0;
    }
}

function updateObstacles() {
    if (Math.random() < 0.02) {
        let obstacle = {
            x: canvas.width,
            y: canvas.height - 50,
            width: 20,
            height: 50
        };
        obstacles.push(obstacle);
    }

    for (let i = obstacles.length - 1; i >= 0; i--) {
        obstacles[i].x -= 5;

        if (obstacles[i].x + obstacles[i].width < 0) {
            obstacles.splice(i, 1);
            score++;
        }
    }
}

function drawDinosaur() {
    ctx.fillStyle = 'green';
    ctx.fillRect(dinosaur.x, dinosaur.y, dinosaur.width, dinosaur.height);
}

function drawObstacles() {
    ctx.fillStyle = 'red';
    for (let obstacle of obstacles) {
        ctx.fillRect(obstacle.x, obstacle.y, obstacle.width, obstacle.height);
    }
}

function checkCollisions() {
    for (let obstacle of obstacles) {
        if (dinosaur.x < obstacle.x + obstacle.width &&
            dinosaur.x + dinosaur.width > obstacle.x &&
            dinosaur.y < obstacle.y + obstacle.height &&
            dinosaur.y + dinosaur.height > obstacle.y) {
            gameOver = true;
            alert('Game Over! Your score: ' + score);
            document.location.reload();
        }
    }
}

function updateScore() {
    ctx.fillStyle = 'black';
    ctx.font = '20px Arial';
    ctx.fillText('Score: ' + score, 10, 20);
}

window.onload = startGame;