// This file contains the JavaScript logic for the multiplayer lobby, managing player connections and game sessions.

const socket = new WebSocket('ws://' + window.location.hostname + ':81'); // Adjust the port if necessary

let players = [];

socket.onopen = function() {
    console.log('Connected to the server');
    // Request to join the lobby
    socket.send(JSON.stringify({ action: 'joinLobby' }));
};

socket.onmessage = function(event) {
    const message = JSON.parse(event.data);
    handleServerMessage(message);
};

function handleServerMessage(message) {
    switch (message.action) {
        case 'playerJoined':
            addPlayer(message.player);
            break;
        case 'playerLeft':
            removePlayer(message.playerId);
            break;
        case 'updatePlayers':
            updatePlayerList(message.players);
            break;
        default:
            console.log('Unknown action:', message.action);
    }
}

function addPlayer(player) {
    players.push(player);
    updatePlayerListDisplay();
}

function removePlayer(playerId) {
    players = players.filter(player => player.id !== playerId);
    updatePlayerListDisplay();
}

function updatePlayerList(playersList) {
    players = playersList;
    updatePlayerListDisplay();
}

function updatePlayerListDisplay() {
    const playerListElement = document.getElementById('playerList');
    playerListElement.innerHTML = ''; // Clear the current list
    players.forEach(player => {
        const li = document.createElement('li');
        li.textContent = player.name;
        playerListElement.appendChild(li);
    });
}

// Function to create a new game session
function createGameSession() {
    socket.send(JSON.stringify({ action: 'createGame' }));
}

// Function to join an existing game session
function joinGameSession(gameId) {
    socket.send(JSON.stringify({ action: 'joinGame', gameId: gameId }));
}

// Event listener for creating a game session
document.getElementById('createGameButton').addEventListener('click', createGameSession);