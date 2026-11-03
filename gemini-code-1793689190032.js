// นำ Webhook URL ของคุณมาวางในเครื่องหมายอัญประกาศ
const DISCORD_WEBHOOK_URL = "https://discord.com/api/webhooks/YOUR_WEBHOOK_URL_HERE";

function notifyDiscord(message) {
    if (!DISCORD_WEBHOOK_URL || DISCORD_WEBHOOK_URL.includes("YOUR_WEBHOOK_URL_HERE")) return;
    
    fetch(DISCORD_WEBHOOK_URL, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ content: message })
    }).catch(err => console.error("Discord Error:", err));
}