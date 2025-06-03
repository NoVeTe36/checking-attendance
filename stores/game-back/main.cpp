#include <Arduino.h>
#include <WiFi.h>
#include <FS.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <WebSocketsServer.h>

// WiFi credentials
const char* ssid = "pumkinpatch2";
const char* password = "Thisisridiculous!";

// Create servers
AsyncWebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

// Button pins (for future 4-button setup)
const int BUTTON_PINS[4] = {2, 4, 5, 18};
bool buttonStates[4] = {false, false, false, false};
bool lastButtonStates[4] = {false, false, false, false};

// Game Center Main Page
const char game_center[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32 Game Center</title>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        
        body {
            font-family: 'Arial', sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            color: white;
        }
        
        .header {
            text-align: center;
            padding: 40px 20px;
            background: rgba(255,255,255,0.1);
            backdrop-filter: blur(10px);
        }
        
        .header h1 {
            font-size: 3em;
            margin-bottom: 10px;
            text-shadow: 2px 2px 4px rgba(0,0,0,0.3);
        }
        
        .subtitle {
            font-size: 1.2em;
            opacity: 0.9;
        }
        
        .game-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(350px, 1fr));
            gap: 30px;
            padding: 40px;
            max-width: 1200px;
            margin: 0 auto;
        }
        
        .game-card {
            background: rgba(255,255,255,0.95);
            color: #333;
            border-radius: 20px;
            padding: 30px;
            text-align: center;
            transition: all 0.3s ease;
            box-shadow: 0 10px 30px rgba(0,0,0,0.2);
        }
        
        .game-card:hover {
            transform: translateY(-10px);
            box-shadow: 0 20px 50px rgba(0,0,0,0.3);
        }
        
        .game-title {
            font-size: 2em;
            font-weight: bold;
            margin-bottom: 15px;
            color: #333;
        }
        
        .game-description {
            margin-bottom: 20px;
            line-height: 1.6;
            color: #666;
        }
        
        .player-info {
            background: #e8f4fd;
            padding: 15px;
            border-radius: 10px;
            margin-bottom: 20px;
            font-weight: bold;
        }
        
        .multiplayer .player-info {
            background: #ffeaa7;
            color: #2d3436;
        }
        
        .singleplayer .player-info {
            background: #a7e8ff;
            color: #2d3436;
        }
        
        .play-btn {
            background: linear-gradient(45deg, #FF6B6B, #4ECDC4);
            color: white;
            padding: 15px 30px;
            text-decoration: none;
            border-radius: 30px;
            display: inline-block;
            font-weight: bold;
            transition: all 0.3s ease;
            box-shadow: 0 4px 15px rgba(0,0,0,0.2);
            border: none;
            cursor: pointer;
            font-size: 1.1em;
        }
        
        .play-btn:hover {
            transform: translateY(-2px);
            box-shadow: 0 6px 20px rgba(0,0,0,0.3);
        }
        
        .features {
            display: flex;
            justify-content: space-around;
            margin-top: 15px;
            padding-top: 15px;
            border-top: 1px solid #eee;
            font-size: 0.9em;
        }
        
        .feature {
            text-align: center;
            color: #666;
        }
        
        .status {
            margin-top: 20px;
            padding: 15px;
            background: rgba(255,255,255,0.1);
            border-radius: 10px;
            text-align: center;
        }
        
        @media (max-width: 768px) {
            .game-grid {
                grid-template-columns: 1fr;
                padding: 20px;
            }
            .header h1 {
                font-size: 2em;
            }
        }
    </style>
</head>
<body>
    <div class="header">
        <h1>ESP32 Game Center</h1>
        <div class="subtitle">Multiplayer Gaming Platform</div>
    </div>
    
    <div class="game-grid">
        <!-- T-Rex Game -->
        <div class="game-card singleplayer">
            <div class="game-title">T-Rex Runner</div>
            <div class="player-info">Single Player</div>
            <div class="game-description">
                Classic endless runner game. Jump over obstacles and survive as long as possible. Simple controls, addictive gameplay.
            </div>
            <div class="features">
                <div class="feature">Endless Gameplay</div>
                <div class="feature">High Scores</div>
                <div class="feature">Progressive Speed</div>
            </div>
            <a href="/games/trex" class="play-btn">Play Now</a>
        </div>
        
        <!-- Snake Game -->
        <div class="game-card singleplayer">
            <div class="game-title">Snake Classic</div>
            <div class="player-info">Single Player</div>
            <div class="game-description">
                Control the snake to eat food and grow longer. Avoid hitting walls or yourself. Classic arcade game with modern controls.
            </div>
            <div class="features">
                <div class="feature">Grow & Survive</div>
                <div class="feature">Score Points</div>
                <div class="feature">Smooth Controls</div>
            </div>
            <a href="/games/snake" class="play-btn">Play Now</a>
        </div>
        
        <!-- Multiplayer Pong -->
        <div class="game-card multiplayer">
            <div class="game-title">Pong Battle</div>
            <div class="player-info">2 Players - Local</div>
            <div class="game-description">
                Classic Pong game for two players. Control paddles with buttons. First to score 10 points wins. Perfect for competitive play.
            </div>
            <div class="features">
                <div class="feature">2 Player Local</div>
                <div class="feature">Button Controls</div>
                <div class="feature">Real-time Battle</div>
            </div>
            <a href="/games/pong" class="play-btn">2 Player Game</a>
        </div>
        
        <!-- Reaction Game -->
        <div class="game-card multiplayer">
            <div class="game-title">Reaction Test</div>
            <div class="player-info">2-4 Players</div>
            <div class="game-description">
                Test your reflexes! Press your button when the screen turns green. Fastest player wins. Great for multiple players.
            </div>
            <div class="features">
                <div class="feature">Up to 4 Players</div>
                <div class="feature">Reflex Testing</div>
                <div class="feature">Quick Rounds</div>
            </div>
            <a href="/games/reaction" class="play-btn">Multiplayer</a>
        </div>
        
        <!-- Quiz Game -->
        <div class="game-card multiplayer">
            <div class="game-title">Quiz Challenge</div>
            <div class="player-info">2-4 Players</div>
            <div class="game-description">
                Answer questions faster than your opponents. Multiple choice questions with button controls. Educational and fun.
            </div>
            <div class="features">
                <div class="feature">Knowledge Test</div>
                <div class="feature">Multiple Categories</div>
                <div class="feature">Competitive</div>
            </div>
            <a href="/games/quiz" class="play-btn">Challenge Friends</a>
        </div>
        
        <!-- Memory Simon -->
        <div class="game-card multiplayer">
            <div class="game-title">Memory Simon</div>
            <div class="player-info">1-4 Players</div>
            <div class="game-description">
                Remember and repeat the sequence of button presses. Each round adds one more step. How far can you go?
            </div>
            <div class="features">
                <div class="feature">Memory Challenge</div>
                <div class="feature">Progressive Difficulty</div>
                <div class="feature">Turn-based</div>
            </div>
            <a href="/games/simon" class="play-btn">Test Memory</a>
        </div>
    </div>
    
    <div class="status">
        <p>Connected Players: <span id="playerCount">0</span></p>
        <p>Server Status: <span id="serverStatus">Online</span></p>
    </div>

    <script>
        // WebSocket connection for real-time updates
        const ws = new WebSocket('ws://' + window.location.hostname + ':81');
        
        ws.onopen = function() {
            document.getElementById('serverStatus').textContent = 'Connected';
        };
        
        ws.onmessage = function(event) {
            const data = JSON.parse(event.data);
            if (data.type === 'playerCount') {
                document.getElementById('playerCount').textContent = data.count;
            }
        };
        
        ws.onclose = function() {
            document.getElementById('serverStatus').textContent = 'Disconnected';
        };
    </script>
</body>
</html>
)rawliteral";

// Simple T-Rex Game
const char trex_game[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>T-Rex Runner</title>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <style>
        body { 
            margin: 0; 
            padding: 20px; 
            font-family: Arial, sans-serif; 
            background: linear-gradient(to bottom, #87CEEB, #98FB98); 
            text-align: center; 
            min-height: 100vh;
        }
        .game-container { 
            background: white; 
            border-radius: 15px; 
            padding: 20px; 
            display: inline-block; 
            box-shadow: 0 10px 30px rgba(0,0,0,0.3);
        }
        #gameCanvas { 
            border: 3px solid #333; 
            background: linear-gradient(to bottom, #87CEEB 70%, #90EE90 100%); 
            border-radius: 10px;
        }
        button { 
            background: linear-gradient(45deg, #FF6B6B, #4ECDC4); 
            color: white; 
            border: none; 
            padding: 12px 24px; 
            border-radius: 25px; 
            font-size: 16px; 
            margin: 10px;
            cursor: pointer;
        }
        .score { 
            font-size: 20px; 
            font-weight: bold; 
            margin: 10px 0; 
            color: #333;
        }
    </style>
</head>
<body>
    <div class="game-container">
        <h1>T-Rex Runner</h1>
        <div class="score">Score: <span id="score">0</span></div>
        <canvas id="gameCanvas" width="800" height="200"></canvas>
        <div>
            <button onclick="startGame()">Start Game</button>
            <button onclick="resetGame()">Reset</button>
            <a href="/" style="color: #4ECDC4; text-decoration: none; margin-left: 20px;">Back to Game Center</a>
        </div>
    </div>
    <script>
        let canvas = document.getElementById('gameCanvas');
        let ctx = canvas.getContext('2d');
        let gameRunning = false;
        let score = 0;
        
        let dino = { x: 50, y: 150, width: 40, height: 40, jumping: false, jumpPower: 0, grounded: true };
        let obstacles = [];
        let gameSpeed = 3;
        let obstacleTimer = 0;

        function startGame() {
            gameRunning = true;
            score = 0;
            obstacles = [];
            obstacleTimer = 0;
            dino.y = 150;
            dino.jumping = false;
            dino.grounded = true;
            dino.jumpPower = 0;
            document.getElementById('score').textContent = score;
            gameLoop();
        }

        function resetGame() {
            gameRunning = false;
            score = 0;
            obstacles = [];
            dino.y = 150;
            dino.jumping = false;
            dino.grounded = true;
            document.getElementById('score').textContent = score;
            draw();
        }

        function jump() {
            if (dino.grounded && gameRunning) {
                dino.jumping = true;
                dino.grounded = false;
                dino.jumpPower = 15;
            }
        }

        function updateDino() {
            if (dino.jumping) {
                dino.y -= dino.jumpPower;
                dino.jumpPower -= 0.8;
                if (dino.y >= 150) {
                    dino.y = 150;
                    dino.jumping = false;
                    dino.grounded = true;
                    dino.jumpPower = 0;
                }
            }
        }

        function updateObstacles() {
            obstacleTimer++;
            if (obstacleTimer > 120) {
                obstacles.push({x: canvas.width, y: 160, width: 20, height: 30});
                obstacleTimer = 0;
            }
            
            for (let i = obstacles.length - 1; i >= 0; i--) {
                obstacles[i].x -= gameSpeed;
                if (obstacles[i].x < -20) {
                    obstacles.splice(i, 1);
                    score += 10;
                    document.getElementById('score').textContent = score;
                }
            }
        }

        function checkCollisions() {
            for (let obstacle of obstacles) {
                if (dino.x < obstacle.x + obstacle.width &&
                    dino.x + dino.width > obstacle.x &&
                    dino.y < obstacle.y + obstacle.height &&
                    dino.y + dino.height > obstacle.y) {
                    gameRunning = false;
                    alert('Game Over! Score: ' + score);
                    return;
                }
            }
        }

        function draw() {
            ctx.clearRect(0, 0, canvas.width, canvas.height);
            
            // Ground
            ctx.fillStyle = '#8B4513';
            ctx.fillRect(0, 190, canvas.width, 10);
            
            // Dinosaur
            ctx.fillStyle = '#4ECDC4';
            ctx.fillRect(dino.x, dino.y, dino.width, dino.height);
            
            // Obstacles
            ctx.fillStyle = '#e17055';
            for (let obstacle of obstacles) {
                ctx.fillRect(obstacle.x, obstacle.y, obstacle.width, obstacle.height);
            }
            
            if (!gameRunning && obstacles.length === 0) {
                ctx.fillStyle = '#333';
                ctx.font = '24px Arial';
                ctx.textAlign = 'center';
                ctx.fillText('Press Start to Play!', canvas.width / 2, canvas.height / 2);
            }
        }

        function gameLoop() {
            if (!gameRunning) return;
            updateDino();
            updateObstacles();
            checkCollisions();
            draw();
            if (gameRunning) requestAnimationFrame(gameLoop);
        }

        document.addEventListener('keydown', (e) => {
            if (e.code === 'Space') {
                e.preventDefault();
                jump();
            }
        });

        canvas.addEventListener('click', jump);
        draw();
    </script>
</body>
</html>
)rawliteral";

const char snake_game[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Snake Classic</title>
    <style>
        body {
            text-align: center;
            font-family: Arial, sans-serif;
            background: linear-gradient(to bottom right, #2E8B57, #98FB98);
            padding: 20px;
            min-height: 100vh;
        }
        .game-container {
            background: rgba(255,255,255,0.95);
            border-radius: 15px;
            padding: 20px;
            display: inline-block;
            box-shadow: 0 10px 30px rgba(0,0,0,0.3);
            color: #333;
        }
        h1 {
            font-size: 36px;
            margin-bottom: 20px;
        }
        .score-display {
            font-size: 24px;
            font-weight: bold;
            margin: 15px 0;
            color: #2E8B57;
        }
        #board {
            border: 3px solid #333;
            margin: 20px auto;
            background-color: #000;
            border-radius: 10px;
        }
        .controls {
            margin: 20px 0;
        }
        button {
            background: linear-gradient(45deg, #32CD32, #228B22);
            color: white;
            border: none;
            padding: 12px 24px;
            border-radius: 25px;
            font-size: 16px;
            font-weight: bold;
            cursor: pointer;
            margin: 10px;
            transition: all 0.3s ease;
            box-shadow: 0 4px 15px rgba(0,0,0,0.2);
        }
        button:hover {
            transform: translateY(-2px);
            box-shadow: 0 6px 20px rgba(0,0,0,0.3);
        }
        .game-over-overlay {
            display: none;
            position: fixed;
            top: 0;
            left: 0;
            width: 100%;
            height: 100%;
            background: rgba(0,0,0,0.8);
            z-index: 1000;
            justify-content: center;
            align-items: center;
        }
        .game-over-content {
            background: white;
            padding: 40px;
            border-radius: 20px;
            text-align: center;
            box-shadow: 0 20px 40px rgba(0,0,0,0.3);
        }
        .instructions {
            background: #f8f9fa;
            padding: 15px;
            border-radius: 10px;
            margin: 15px 0;
            border-left: 4px solid #32CD32;
        }
    </style>
</head>
<body>
    <div class="game-container">
        <h1>Snake Classic</h1>
        <div class="score-display">Score: <span id="score">0</span></div>
        <div class="score-display">High Score: <span id="highScore">0</span></div>
        <canvas id="board"></canvas>
        <div class="controls">
            <button onclick="startGame()">Start Game</button>
            <button onclick="restartGame()">Restart</button>
            <button onclick="pauseGame()">Pause</button>
        </div>
        <div class="instructions">
            <strong>Controls:</strong> Arrow Keys to move | Space to pause
        </div>
        <a href="/" style="color: #4ECDC4; text-decoration: none; font-weight: bold;">← Back to Game Center</a>
    </div>

    <div class="game-over-overlay" id="gameOverOverlay">
        <div class="game-over-content">
            <h2>Game Over!</h2>
            <p>Your Score: <span id="finalScore">0</span></p>
            <p>High Score: <span id="finalHighScore">0</span></p>
            <button onclick="restartGame()">Play Again</button>
            <button onclick="goHome()">Game Center</button>
        </div>
    </div>

    <script>
        let blockSize = 25;
        let total_row = 17;
        let total_col = 17;
        let board;
        let context;

        let snakeX = blockSize * 5;
        let snakeY = blockSize * 5;

        let speedX = 0;
        let speedY = 0;

        let snakeBody = [];

        let foodX;
        let foodY;

        let gameOver = false;
        let gameStarted = false;
        let gamePaused = false;
        let score = 0;
        let highScore = localStorage.getItem('snakeHighScore') || 0;
        let gameInterval;

        window.onload = function () {
            board = document.getElementById("board");
            board.height = total_row * blockSize;
            board.width = total_col * blockSize;
            context = board.getContext("2d");

            document.getElementById('highScore').textContent = highScore;
            
            placeFood();
            document.addEventListener("keyup", changeDirection);
            document.addEventListener("keydown", function(e) {
                if (e.code === 'Space') {
                    e.preventDefault();
                    pauseGame();
                }
            });
            
            // Initial draw
            draw();
        }

        function startGame() {
            if (gameStarted && !gameOver) return;
            
            gameStarted = true;
            gameOver = false;
            gamePaused = false;
            
            // Reset game state
            snakeX = blockSize * 5;
            snakeY = blockSize * 5;
            speedX = 0;
            speedY = 0;
            snakeBody = [];
            score = 0;
            
            document.getElementById('score').textContent = score;
            document.getElementById('gameOverOverlay').style.display = 'none';
            
            placeFood();
            
            // Start game loop
            if (gameInterval) clearInterval(gameInterval);
            gameInterval = setInterval(update, 1000 / 10);
        }

        function restartGame() {
            // Stop current game
            if (gameInterval) clearInterval(gameInterval);
            
            // Reset all game variables
            gameStarted = false;
            gameOver = false;
            gamePaused = false;
            snakeX = blockSize * 5;
            snakeY = blockSize * 5;
            speedX = 0;
            speedY = 0;
            snakeBody = [];
            score = 0;
            
            // Update UI
            document.getElementById('score').textContent = score;
            document.getElementById('gameOverOverlay').style.display = 'none';
            
            // Reset food and redraw
            placeFood();
            draw();
            
            // Start new game
            startGame();
        }

        function pauseGame() {
            if (!gameStarted || gameOver) return;
            
            gamePaused = !gamePaused;
            
            if (gamePaused) {
                clearInterval(gameInterval);
            } else {
                gameInterval = setInterval(update, 1000 / 10);
            }
        }

        function endGame() {
            gameOver = true;
            gameStarted = false;
            clearInterval(gameInterval);
            
            // Update high score
            if (score > highScore) {
                highScore = score;
                localStorage.setItem('snakeHighScore', highScore);
                document.getElementById('highScore').textContent = highScore;
            }
            
            // Show game over overlay
            document.getElementById('finalScore').textContent = score;
            document.getElementById('finalHighScore').textContent = highScore;
            document.getElementById('gameOverOverlay').style.display = 'flex';
        }

        function goHome() {
            window.location.href = '/';
        }

        function update() {
            if (gameOver || gamePaused) return;

            // Clear canvas
            context.fillStyle = "black";
            context.fillRect(0, 0, board.width, board.height);

            // Draw food
            context.fillStyle = "red";
            context.fillRect(foodX, foodY, blockSize, blockSize);

            // Check if snake ate food
            if (snakeX == foodX && snakeY == foodY) {
                snakeBody.push([foodX, foodY]);
                score += 10;
                document.getElementById('score').textContent = score;
                placeFood();
            }

            // Move snake body
            for (let i = snakeBody.length - 1; i > 0; i--) {
                snakeBody[i] = snakeBody[i - 1];
            }
            if (snakeBody.length) {
                snakeBody[0] = [snakeX, snakeY];
            }

            // Move snake head
            snakeX += speedX * blockSize;
            snakeY += speedY * blockSize;

            // Draw snake
            context.fillStyle = "lime";
            context.fillRect(snakeX, snakeY, blockSize, blockSize);
            for (let i = 0; i < snakeBody.length; i++) {
                context.fillRect(snakeBody[i][0], snakeBody[i][1], blockSize, blockSize);
            }

            // Check wall collision
            if (snakeX < 0 || snakeX >= total_col * blockSize || snakeY < 0 || snakeY >= total_row * blockSize) {
                endGame();
                return;
            }

            // Check self collision
            for (let i = 0; i < snakeBody.length; i++) {
                if (snakeX == snakeBody[i][0] && snakeY == snakeBody[i][1]) {
                    endGame();
                    return;
                }
            }
        }

        function draw() {
            // Clear canvas
            context.fillStyle = "black";
            context.fillRect(0, 0, board.width, board.height);

            // Draw food
            context.fillStyle = "red";
            context.fillRect(foodX, foodY, blockSize, blockSize);

            // Draw snake
            context.fillStyle = "lime";
            context.fillRect(snakeX, snakeY, blockSize, blockSize);
            for (let i = 0; i < snakeBody.length; i++) {
                context.fillRect(snakeBody[i][0], snakeBody[i][1], blockSize, blockSize);
            }

            // Draw start message
            if (!gameStarted) {
                context.fillStyle = "white";
                context.font = "24px Arial";
                context.textAlign = "center";
                context.fillText("Press Start to Play!", board.width / 2, board.height / 2);
            } else if (gamePaused) {
                context.fillStyle = "white";
                context.font = "24px Arial";
                context.textAlign = "center";
                context.fillText("PAUSED", board.width / 2, board.height / 2);
            }
        }

        function changeDirection(e) {
            if (!gameStarted || gameOver || gamePaused) return;

            if (e.code == "ArrowUp" && speedY != 1) {
                speedX = 0;
                speedY = -1;
            } else if (e.code == "ArrowDown" && speedY != -1) {
                speedX = 0;
                speedY = 1;
            } else if (e.code == "ArrowLeft" && speedX != 1) {
                speedX = -1;
                speedY = 0;
            } else if (e.code == "ArrowRight" && speedX != -1) {
                speedX = 1;
                speedY = 0;
            }
        }

        function placeFood() {
            do {
                foodX = Math.floor(Math.random() * total_col) * blockSize;
                foodY = Math.floor(Math.random() * total_row) * blockSize;
            } while (isFoodOnSnake());
        }

        function isFoodOnSnake() {
            if (foodX === snakeX && foodY === snakeY) return true;
            for (let segment of snakeBody) {
                if (foodX === segment[0] && foodY === segment[1]) return true;
            }
            return false;
        }
    </script>
</body>
</html>
)rawliteral";



// WebSocket event handler
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[%u] Disconnected!\n", num);
            break;
        case WStype_CONNECTED:
            {
                IPAddress ip = webSocket.remoteIP(num);
                Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
                
                // Send player count update
                String msg = "{\"type\":\"playerCount\",\"count\":" + String(webSocket.connectedClients()) + "}";
                webSocket.broadcastTXT(msg);
            }
            break;
        case WStype_TEXT:
            Serial.printf("[%u] Received Text: %s\n", num, payload);
            break;
        default:
            break;
    }
}

void setupWebServer() {
    // Main game center page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", game_center);
    });
    
    // T-Rex game
    server.on("/games/trex", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", trex_game);
    });
    
    // Placeholder for other games
    server.on("/games/snake", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", snake_game);
    });
    
    server.on("/games/pong", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", "<h1>Pong Game - Coming Soon!</h1><a href='/'>Back to Game Center</a>");
    });
    
    server.on("/games/reaction", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", "<h1>Reaction Game - Coming Soon!</h1><a href='/'>Back to Game Center</a>");
    });
    
    server.on("/games/quiz", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", "<h1>Quiz Game - Coming Soon!</h1><a href='/'>Back to Game Center</a>");
    });
    
    server.on("/games/simon", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", "<h1>Simon Game - Coming Soon!</h1><a href='/'>Back to Game Center</a>");
    });
    
    // 404 handler
    server.onNotFound([](AsyncWebServerRequest *request){
        request->redirect("/");
    });
    
    server.begin();
    Serial.println("HTTP server started");
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    // Initialize button pins
    for (int i = 0; i < 4; i++) {
        pinMode(BUTTON_PINS[i], INPUT_PULLUP);
    }
    
    // Connect to WiFi
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConnected to WiFi!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    
    // Setup web server and WebSocket
    setupWebServer();
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
    
    Serial.println("Game Center started!");
    Serial.println("Access at: http://" + WiFi.localIP().toString());
}

void loop() {
    webSocket.loop();
    
    // Read button states (for future multiplayer games)
    for (int i = 0; i < 4; i++) {
        buttonStates[i] = !digitalRead(BUTTON_PINS[i]); // Inverted because of INPUT_PULLUP
        
        // Detect button press (rising edge)
        if (buttonStates[i] && !lastButtonStates[i]) {
            Serial.println("Button " + String(i + 1) + " pressed");
            
            // Send button press to all connected clients
            String msg = "{\"type\":\"buttonPress\",\"button\":" + String(i + 1) + "}";
            webSocket.broadcastTXT(msg);
        }
        
        lastButtonStates[i] = buttonStates[i];
    }
    
    delay(10); // Small delay for button debouncing
}