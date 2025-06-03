// This file contains the main JavaScript logic for the game center, handling user interactions and game navigation.

document.addEventListener('DOMContentLoaded', function() {
    const gameLinks = document.querySelectorAll('.game-link');

    gameLinks.forEach(link => {
        link.addEventListener('click', function(event) {
            event.preventDefault();
            const gameUrl = this.getAttribute('href');
            loadGame(gameUrl);
        });
    });

    function loadGame(url) {
        const gameContainer = document.getElementById('game-container');
        fetch(url)
            .then(response => {
                if (!response.ok) {
                    throw new Error('Network response was not ok');
                }
                return response.text();
            })
            .then(html => {
                gameContainer.innerHTML = html;
                initializeGame();
            })
            .catch(error => {
                console.error('There was a problem with the fetch operation:', error);
            });
    }

    function initializeGame() {
        // Initialize game-specific scripts or settings here
        const script = document.createElement('script');
        script.src = 'games/dinosaur/dinosaur.js'; // Adjust based on the game loaded
        document.body.appendChild(script);
    }
});