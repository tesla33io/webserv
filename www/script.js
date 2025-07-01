class DataApp {
    constructor() {
        this.elements = this.getElements();
        this.init();
    }

    getElements() {
        const form = document.getElementById('dataForm');
        const textInput = document.getElementById('textInput');
        const submitBtn = document.getElementById('submitBtn');
        const responseContent = document.getElementById('responseContent');
        const successMessage = document.getElementById('successMessage');

        if (!form || !textInput || !submitBtn || !responseContent || !successMessage) {
            throw new Error('Required DOM elements not found');
        }

        return {
            form,
            textInput,
            submitBtn,
            responseContent,
            successMessage
        };
    }

    init() {
        this.elements.form.addEventListener('submit', this.handleSubmit.bind(this));
        this.elements.textInput.addEventListener('input', this.handleInput.bind(this));
    }

    async handleSubmit(e) {
        e.preventDefault();

        const inputText = this.elements.textInput.value.trim();

        if (!inputText) {
            this.showError('Please enter some text before submitting.');
            return;
        }

        await this.submitData(inputText);
    }

    handleInput() {
        if (this.elements.textInput.style.borderColor === 'rgb(239, 68, 68)') {
            this.elements.textInput.style.borderColor = '';
            this.elements.textInput.style.boxShadow = '';
        }
    }

    async submitData(text) {
        this.setLoadingState(true);

        try {
            const response = await this.sendToServer(text);
            this.showSuccess(text, response);
            this.showSuccessMessage();
            this.elements.textInput.value = '';
        } catch (error) {
            this.showError(`Error: ${error instanceof Error ? error.message : 'Unknown error'}`);
        } finally {
            this.setLoadingState(false);
        }
    }

    setLoadingState(loading) {
        this.elements.submitBtn.disabled = loading;
        this.elements.submitBtn.textContent = loading ? 'Sending...' : 'Send to Server';

        if (loading) {
            this.elements.responseContent.innerHTML = '<span class="loading">Sending data to server...</span>';
        }
    }

    showSuccess(inputText, response) {
        this.elements.responseContent.innerHTML = `
            <div class="success">
                <strong>✅ Success!</strong><br><br>
                <strong>Sent:</strong> ${this.escapeHtml(inputText)}<br>
                <strong>Server Response:</strong> ${this.escapeHtml(response.message)}<br>
                <strong>Timestamp:</strong> ${this.escapeHtml(response.timestamp)}<br>
                <strong>Text Length:</strong> ${response.length} characters
            </div>
        `;
    }

    showError(message) {
        this.elements.responseContent.innerHTML = `<span class="error">${this.escapeHtml(message)}</span>`;

        this.elements.textInput.style.borderColor = '#ef4444';
        this.elements.textInput.style.boxShadow = '0 0 0 4px rgba(239, 68, 68, 0.1)';

        setTimeout(() => {
            this.elements.textInput.style.borderColor = '';
            this.elements.textInput.style.boxShadow = '';
        }, 3000);
    }

    showSuccessMessage() {
        this.elements.successMessage.classList.add('show');

        setTimeout(() => {
            this.elements.successMessage.classList.remove('show');
        }, 4000);
    }

    escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }

    async sendToServer(text) {
        await this.delay(1000);

        const response = {
            message: `Processed: "${text}" - Server received your message successfully!`,
            timestamp: new Date().toLocaleString(),
            length: text.length
        };

        return response;

        /*
        // Use this for real server communication:
        try {
            const response = await fetch('YOUR_SERVER_ENDPOINT', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({ text })
            });

            if (!response.ok) {
                throw new Error(`Server error: ${response.status} ${response.statusText}`);
            }

            return await response.json();
        } catch (error) {
            if (error instanceof TypeError) {
                throw new Error('Network error - please check your connection');
            }
            throw error;
        }
        */
    }

    delay(ms) {
        return new Promise(resolve => setTimeout(resolve, ms));
    }
}

document.addEventListener('DOMContentLoaded', () => {
    try {
        new DataApp();
    } catch (error) {
        console.error('Failed to initialize app:', error);
    }
});
