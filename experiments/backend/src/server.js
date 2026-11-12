const app = require('./app');
const config = require('./config');

app.listen(config.port, () => {
  console.log(`Logix backend running on http://localhost:${config.port}`);
});
