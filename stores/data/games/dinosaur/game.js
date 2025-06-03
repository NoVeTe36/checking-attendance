class DinosaurGame {
    constructor() {
        this.canvas = document.getElementById('gameCanvas');
        this.ctx = this.canvas.getContext('2d');
        this.scoreElement = document.getElementById('score');
        this.gameOverElement = document.getElementById('gameOver');
        this.finalScoreElement = document.getElementById('finalScore');
        
        this.gameRunning = false;
        this.score = 0;
        this.gameSpeed = 3;
        
        // Dinosaur properties
        this.dino = {
            x: 50,
            y: 150,
            width: 40,
            height: 40,
            jumping: false,
            jumpPower: 0,
            grounded: true
        };
        
        // Obstacles array
        this.obstacles = [];
        this.obstacleTimer = 0;
        
        this.setupEventListeners();
        this.gameLoop();
    }
    
    setupEventListeners() {
        document.addEventListener('keydown', (e) => {
            if (e.code === 'Space' && this.gameRunning) {
                e.preventDefault();
                this.jump();
            }
        });
        
        this.canvas.addEventListener('click', () => {
            if (this.gameRunning) {
                this.jump();
            }
        });
        
        document.getElementById('startBtn').addEventListener('click', () => {
            this.startGame();
        });
        
        document.getElementById('resetBtn').addEventListener('click', () => {
            this.resetGame();
        });
    }
    
    startGame() {
        this.gameRunning = true;
        this.score = 0;
        this.obstacles = [];
        this.dino.y = 150;
        this.dino.jumping = false;
        this.dino.jumpPower = 0;
        this.dino.grounded = true;
        this.gameOverElement.style.display = 'none';
    }
    
    resetGame() {
        this.gameRunning = false;
        this.score = 0;
        this.obstacles = [];
        this.updateScore();
        this.gameOverElement.style.display = 'none';
    }
    
    jump() {
        if (this.dino.grounded) {
            this.dino.jumping = true;
            this.dino.grounded = false;
            this.dino.jumpPower = 15;
        }
    }
    
    updateDinosaur() {
        if (this.dino.jumping) {
            this.dino.y -= this.dino.jumpPower;
            this.dino.jumpPower -= 0.8;
            
            if (this.dino.y >= 150) {
                this.dino.y = 150;
                this.dino.jumping = false;
                this.dino.grounded = true;
                this.dino.jumpPower = 0;
            }
        }
    }
    
    updateObstacles() {
        this.obstacleTimer++;
        
        if (this.obstacleTimer > 100) {
            this.obstacles.push({
                x: this.canvas.width,
                y: 160,
                width: 20,
                height: 30
            });
            this.obstacleTimer = 0;
        }
        
        for (let i = this.obstacles.length - 1; i >= 0; i--) {
            this.obstacles[i].x -= this.gameSpeed;
            
            if (this.obstacles[i].x + this.obstacles[i].width < 0) {
                this.obstacles.splice(i, 1);
                this.score += 10;
                this.updateScore();
            }
        }
    }
    
    checkCollisions() {
        for (let obstacle of this.obstacles) {
            if (this.dino.x < obstacle.x + obstacle.width &&
                this.dino.x + this.dino.width > obstacle.x &&
                this.dino.y < obstacle.y + obstacle.height &&
                this.dino.y + this.dino.height > obstacle.y) {
                this.gameOver();
            }
        }
    }
    
    gameOver() {
        this.gameRunning = false;
        this.finalScoreElement.textContent = this.score;
        this.gameOverElement.style.display = 'block';
    }
    
    updateScore() {
        this.scoreElement.textContent = this.score;
    }
    
    draw() {
        // Clear canvas
        this.ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);
        
        // Draw ground
        this.ctx.fillStyle = '#2d3436';
        this.ctx.fillRect(0, 190, this.canvas.width, 10);
        
        // Draw dinosaur
        this.ctx.fillStyle = '#00b894';
        this.ctx.fillRect(this.dino.x, this.dino.y, this.dino.width, this.dino.height);
        
        // Draw obstacles
        this.ctx.fillStyle = '#e17055';
        for (let obstacle of this.obstacles) {
            this.ctx.fillRect(obstacle.x, obstacle.y, obstacle.width, obstacle.height);
        }
        
        // Draw game status
        if (!this.gameRunning && this.obstacles.length === 0) {
            this.ctx.fillStyle = '#636e72';
            this.ctx.font = '24px Arial';
            this.ctx.textAlign = 'center';
            this.ctx.fillText('Press Start to Play!', this.canvas.width / 2, this.canvas.height / 2);
        }
    }
    
    gameLoop() {
        if (this.gameRunning) {
            this.updateDinosaur();
            this.updateObstacles();
            this.checkCollisions();
        }
        
        this.draw();
        requestAnimationFrame(() => this.gameLoop());
    }
}

// Start the game when page loads
window.addEventListener('load', () => {
    new DinosaurGame();
});

// Global function for reset button in game over screen
function resetGame() {
    location.reload();
}