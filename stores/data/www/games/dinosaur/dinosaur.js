// This file contains the JavaScript logic for the dinosaur game, including game mechanics and player interactions.

let canvas = document.getElementById('gameCanvas');
let ctx = canvas.getContext('2d');
let gameRunning = false;
let score = 0;
let highScore = localStorage.getItem('dinoHighScore') || 0;
let animationId;
let gameSpeed = 3;

const dinosaur = {
    x: 50,
    y: 150,
    width: 40,
    height: 40,
    jumping: false,
    ducking: false,
    jumpPower: 0,
    grounded: true,
    originalHeight: 40
};

let obstacles = [];
let obstacleTimer = 0;
let clouds = [];
let cloudTimer = 0;

const obstacleTypes = {
    CACTUS_SMALL: { width: 20, height: 35, y: 155, flying: false, color: '#228B22' },
    CACTUS_LARGE: { width: 30, height: 50, y: 140, flying: false, color: '#006400' },
    BIRD: { width: 35, height: 25, y: 100, flying: true, color: '#8B4513' },
    PTERODACTYL: { width: 40, height: 30, y: 80, flying: true, color: '#4B0082' }
};

document.getElementById('highScore').textContent = highScore;

function startGame() {
    if (gameRunning) return;

    gameRunning = true;
    score = 0;
    obstacles = [];
    clouds = [];
    obstacleTimer = 0;
    cloudTimer = 0;
    gameSpeed = 3;
    dinosaur.y = 150;
    dinosaur.jumping = false;
    dinosaur.ducking = false;
    dinosaur.grounded = true;
    dinosaur.jumpPower = 0;
    dinosaur.height = dinosaur.originalHeight;
    document.getElementById('gameOver').style.display = 'none';
    document.getElementById('score').textContent = score;
    gameLoop();
}

function resetGame() {
    gameRunning = false;
    if (animationId) cancelAnimationFrame(animationId);

    score = 0;
    obstacles = [];
    clouds = [];
    gameSpeed = 3;
    dinosaur.y = 150;
    dinosaur.jumping = false;
    dinosaur.ducking = false;
    dinosaur.grounded = true;
    dinosaur.height = dinosaur.originalHeight;
    document.getElementById('gameOver').style.display = 'none';
    document.getElementById('score').textContent = score;
    draw();
}

function jump() {
    if (dinosaur.grounded && !dinosaur.ducking && gameRunning) {
        dinosaur.jumping = true;
        dinosaur.grounded = false;
        dinosaur.jumpPower = 15;
    }
}

function duck(isDucking) {
    if (gameRunning && !dinosaur.jumping) {
        dinosaur.ducking = isDucking;
        dinosaur.height = isDucking ? 20 : dinosaur.originalHeight;
        dinosaur.y = isDucking ? 170 : 150;
    }
}

function updateDino() {
    if (dinosaur.jumping) {
        dinosaur.y -= dinosaur.jumpPower;
        dinosaur.jumpPower -= 0.8;

        if (dinosaur.y >= 150) {
            dinosaur.y = 150;
            dinosaur.jumping = false;
            dinosaur.grounded = true;
            dinosaur.jumpPower = 0;
        }
    }
}

function createObstacle() {
    const types = Object.keys(obstacleTypes);
    const randomType = types[Math.floor(Math.random() * types.length)];
    const obstacleData = obstacleTypes[randomType];

    return {
        x: canvas.width,
        y: obstacleData.y,
        width: obstacleData.width,
        height: obstacleData.height,
        flying: obstacleData.flying,
        color: obstacleData.color,
        type: randomType
    };
}

function createCloud() {
    return {
        x: canvas.width,
        y: Math.random() * 60 + 20,
        width: 60,
        height: 30,
        speed: 0.5
    };
}

function updateObstacles() {
    obstacleTimer++;
    if (obstacleTimer > Math.max(80 - score / 10, 40)) {
        obstacles.push(createObstacle());
        obstacleTimer = 0;
    }

    for (let i = obstacles.length - 1; i >= 0; i--) {
        obstacles[i].x -= gameSpeed;

        if (obstacles[i].x < -obstacles[i].width) {
            obstacles.splice(i, 1);
            score += 10;
            document.getElementById('score').textContent = score;

            if (score % 100 === 0) {
                gameSpeed += 0.5;
            }
        }
    }
}

function updateClouds() {
    cloudTimer++;
    if (cloudTimer > 200) {
        clouds.push(createCloud());
        cloudTimer = 0;
    }

    for (let i = clouds.length - 1; i >= 0; i--) {
        clouds[i].x -= clouds[i].speed;

        if (clouds[i].x < -clouds[i].width) {
            clouds.splice(i, 1);
        }
    }
}

function checkCollisions() {
    for (let obstacle of obstacles) {
        if (dinosaur.x < obstacle.x + obstacle.width - 5 &&
            dinosaur.x + dinosaur.width > obstacle.x + 5 &&
            dinosaur.y < obstacle.y + obstacle.height - 5 &&
            dinosaur.y + dinosaur.height > obstacle.y + 5) {
            gameOver();
            return;
        }
    }
}

function gameOver() {
    gameRunning = false;

    if (score > highScore) {
        highScore = score;
        localStorage.setItem('dinoHighScore', highScore);
        document.getElementById('highScore').textContent = highScore;
    }

    document.getElementById('finalScore').textContent = score;
    document.getElementById('finalHighScore').textContent = highScore;
    document.getElementById('gameOver').style.display = 'flex';
}

function drawDino() {
    ctx.fillStyle = dinosaur.ducking ? '#FF6B6B' : '#4ECDC4';
    ctx.fillRect(dinosaur.x, dinosaur.y, dinosaur.width, dinosaur.height);

    ctx.fillStyle = 'white';
    ctx.fillRect(dinosaur.x + 25, dinosaur.y + 8, 8, 6);
    ctx.fillStyle = 'black';
    ctx.fillRect(dinosaur.x + 28, dinosaur.y + 10, 3, 3);

    ctx.fillStyle = dinosaur.ducking ? '#FF6B6B' : '#4ECDC4';
    if (gameRunning && Math.floor(Date.now() / 200) % 2) {
        ctx.fillRect(dinosaur.x + 5, dinosaur.y + dinosaur.height, 8, 10);
        ctx.fillRect(dinosaur.x + 25, dinosaur.y + dinosaur.height, 8, 10);
    } else {
        ctx.fillRect(dinosaur.x + 10, dinosaur.y + dinosaur.height, 8, 10);
        ctx.fillRect(dinosaur.x + 20, dinosaur.y + dinosaur.height, 8, 10);
    }
}

function drawObstacles() {
    for (let obstacle of obstacles) {
        ctx.fillStyle = obstacle.color;

        if (obstacle.type === 'BIRD' || obstacle.type === 'PTERODACTYL') {
            const wingFlap = Math.sin(Date.now() / 100) * 5;
            ctx.fillRect(obstacle.x, obstacle.y + wingFlap, obstacle.width, obstacle.height);

            ctx.fillStyle = obstacle.color;
            ctx.fillRect(obstacle.x + 5, obstacle.y + wingFlap - 5, 25, 8);
            ctx.fillRect(obstacle.x + 5, obstacle.y + wingFlap + 20, 25, 8);
        } else {
            ctx.fillRect(obstacle.x, obstacle.y, obstacle.width, obstacle.height);

            if (obstacle.type === 'CACTUS_LARGE') {
                ctx.fillRect(obstacle.x - 8, obstacle.y + 15, 15, 8);
                ctx.fillRect(obstacle.x + obstacle.width - 7, obstacle.y + 25, 15, 8);
            }
        }
    }
}

function drawClouds() {
    ctx.fillStyle = 'rgba(255, 255, 255, 0.8)';
    for (let cloud of clouds) {
        ctx.beginPath();
        ctx.arc(cloud.x, cloud.y, 15, 0, Math.PI * 2);
        ctx.arc(cloud.x + 20, cloud.y, 20, 0, Math.PI * 2);
        ctx.arc(cloud.x + 40, cloud.y, 15, 0, Math.PI * 2);
        ctx.fill();
    }
}

function drawGround() {
    ctx.fillStyle = '#8B4513';
    ctx.fillRect(0, 190, canvas.width, 10);

    ctx.fillStyle = '#654321';
    for (let i = 0; i < canvas.width; i += 40) {
        ctx.fillRect(i + (score % 40), 195, 20, 3);
    }
}

function draw() {
    ctx.clearRect(0, 0, canvas.width, canvas.height);

    const gradient = ctx.createLinearGradient(0, 0, 0, canvas.height);
    gradient.addColorStop(0, '#87CEEB');
    gradient.addColorStop(0.7, '#E0F6FF');
    gradient.addColorStop(1, '#98FB98');
    ctx.fillStyle = gradient;
    ctx.fillRect(0, 0, canvas.width, canvas.height);

    drawClouds();
    drawGround();
    drawDino();
    drawObstacles();

    if (!gameRunning && obstacles.length === 0) {
        ctx.fillStyle = '#333';
        ctx.font = 'bold 24px Arial';
        ctx.textAlign = 'center';
        ctx.fillText('🦕 Click Start Game to Begin!', canvas.width / 2, canvas.height / 2);
        ctx.font = '16px Arial';
        ctx.fillText('Jump over ground obstacles, duck under flying ones!', canvas.width / 2, canvas.height / 2 + 30);
    }
}

function gameLoop() {
    if (!gameRunning) return;

    updateDino();
    updateObstacles();
    updateClouds();
    checkCollisions();
    draw();

    if (gameRunning) {
        animationId = requestAnimationFrame(gameLoop);
    }
}

document.addEventListener('keydown', (e) => {
    if (e.code === 'Space') {
        e.preventDefault();
        jump();
    } else if (e.code === 'ArrowDown') {
        e.preventDefault();
        duck(true);
    }
});

document.addEventListener('keyup', (e) => {
    if (e.code === 'ArrowDown') {
        e.preventDefault();
        duck(false);
    }
});

canvas.addEventListener('click', jump);

draw();