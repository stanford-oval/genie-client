(function() {
    function updateAuthMode() {
        const auth_mode = document.getElementById('config-auth-mode').value;

        if (auth_mode === 'home-assistant' || auth_mode === 'bearer')
            document.getElementById('config-access-token-group').classList.remove('d-none');
        else
            document.getElementById('config-access-token-group').classList.add('d-none');
    }

    updateAuthMode();
    document.getElementById('config-auth-mode').onchange = function() {
        updateAuthMode();
    };
})()
