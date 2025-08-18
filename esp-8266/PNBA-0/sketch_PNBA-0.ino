// ESP8266 Basketball Match Predictor Web Server
// Accede via navegador web para introducir estadísticas de dos equipos

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// --- Configuración Wi-Fi ---
const char* ssid     = "SSID";
const char* password = "PASSWORD";

ESP8266WebServer server(80);

struct Team {
  String name;
  float GP, W, L, W_pct;
  float Min, PTS;
  float FGM, FGA, FG_pct;
  float TP_M, TP_A, TP_pct;
  float FT_M, FT_A, FT_pct;
  float OREB, DREB, REB;
  float AST, TOV;
  float STL, BLK;
  float BLKA, PF, PFD, plus_minus;

  // Métricas derivadas
  float assist_ratio, turnover_ratio, true_shooting_pct;
  float rebound_pct, net_rating, elo_like, raptor_like;
};

void computeAdvancedMetrics(Team &t) {
  t.assist_ratio = t.PTS > 0 ? t.AST / t.PTS : 0;
  t.turnover_ratio = (t.FGA + 0.44 * t.FT_A + t.TOV) > 0 ? t.TOV / (t.FGA + 0.44 * t.FT_A + t.TOV) : 0;
  t.true_shooting_pct = (2 * (t.FGA + 0.44 * t.FT_A)) > 0 ? t.PTS / (2 * (t.FGA + 0.44 * t.FT_A)) : 0;
  t.rebound_pct = (t.OREB + t.DREB) / (t.Min * 5.0);
  float ortg = (t.TOV + t.FGA + 0.44 * t.FT_A - t.OREB) > 0 ? t.PTS / (t.TOV + t.FGA + 0.44 * t.FT_A - t.OREB) : 0;
  float drtg = 100.0 - t.plus_minus;  // Aproximación inversa
  t.net_rating = ortg - drtg;

  // Estimaciones tipo ELO y RAPTOR
  t.elo_like = t.W_pct * 400 + t.plus_minus * 5 + t.FG_pct * 100 + t.STL * 2 + t.BLK * 2 + t.REB;
  float raptor_off = (t.AST + t.FGM + t.TP_M + t.FT_pct) / (t.TOV + 1);
  float raptor_def = t.STL + t.BLK - t.PF - t.BLKA;
  t.raptor_like = raptor_off + raptor_def;
}

double computeScore(const Team &t) {
  return
    t.W_pct * 6 +
    t.PTS * 1 +
    t.FG_pct * 2 +
    t.true_shooting_pct * 4 +
    (t.AST / (t.TOV + 1)) * 3 +
    t.STL * 2 + t.BLK * 2 +
    t.rebound_pct * 100 +
    t.elo_like * 0.1 +
    t.raptor_like * 2 +
    t.net_rating * 0.5 +
    t.plus_minus;
}

const char* htmlForm = R"rawliteral(
<!DOCTYPE html>
<html>
<head><meta charset="utf-8"><title>Predictor Baloncesto</title></head>
<body>
  <h1><img src="https://static-nw-production.sportingnews.com/media/filer_public/82/58/82581755-dea4-4992-902b-c1647cd0782c/menu-item-logosnbanba.png.48x48_q85_crop.png">Predicción de Partidos:</h1>

  <p>Pega la línea completa de <a href="https://www.nba.com/stats/teams/traditional">estadísticas</a> para cada equipo (nombre + 26 valores):</p>
  <form action="/predict" method="post">
    <fieldset><legend>Equipo A</legend><textarea name="a_line" rows="3" cols="90"></textarea></fieldset>
    <fieldset><legend>Equipo B</legend><textarea name="b_line" rows="3" cols="90"></textarea></fieldset>
    <input type="submit" value="Predecir">
  </form>
</body>
</html>
)rawliteral";

bool parseLine(const String &line, Team &t) {
  String s = line; s.replace('\t', ' ');
  std::vector<String> tokens;
  int start = 0;
  for (int i = 0; i <= s.length(); ++i) {
    if (i == s.length() || s[i] == ' ') {
      if (i - start > 0) tokens.push_back(s.substring(start, i));
      start = i + 1;
    }
  }
  if (tokens.size() < 27) return false;
  t.name       = tokens[0];
  t.GP         = tokens[1].toFloat();
  t.W          = tokens[2].toFloat();
  t.L          = tokens[3].toFloat();
  t.W_pct      = tokens[4].toFloat();
  t.Min        = tokens[5].toFloat();
  t.PTS        = tokens[6].toFloat();
  t.FGM        = tokens[7].toFloat();
  t.FGA        = tokens[8].toFloat();
  t.FG_pct     = tokens[9].toFloat();
  t.TP_M       = tokens[10].toFloat();
  t.TP_A       = tokens[11].toFloat();
  t.TP_pct     = tokens[12].toFloat();
  t.FT_M       = tokens[13].toFloat();
  t.FT_A       = tokens[14].toFloat();
  t.FT_pct     = tokens[15].toFloat();
  t.OREB       = tokens[16].toFloat();
  t.DREB       = tokens[17].toFloat();
  t.REB        = tokens[18].toFloat();
  t.AST        = tokens[19].toFloat();
  t.TOV        = tokens[20].toFloat();
  t.STL        = tokens[21].toFloat();
  t.BLK        = tokens[22].toFloat();
  t.BLKA       = tokens[23].toFloat();
  t.PF         = tokens[24].toFloat();
  t.PFD        = tokens[25].toFloat();
  t.plus_minus = tokens[26].toFloat();
  return true;
}

void handleRoot() {
  server.send(200, "text/html", htmlForm);
}

void handlePredict() {
  Team A, B;
  String lineA = server.arg("a_line");
  String lineB = server.arg("b_line");
  if (!parseLine(lineA, A) || !parseLine(lineB, B)) {
    server.send(400, "text/plain", "Error: formato inválido. Asegúrate de pegar la línea completa con 27 valores por equipo.");
    return;
  }

  computeAdvancedMetrics(A);
  computeAdvancedMetrics(B);

  double scoreA = computeScore(A);
  double scoreB = computeScore(B);

  auto teamToHTML = [](const Team& t) {
    String html = "<h2>" + t.name + "</h2><ul>";
    html += "<li><strong>assist_ratio:</strong> " + String(t.assist_ratio, 3) + " → mide asistencias por punto (más = mejor juego en equipo)</li>";
    html += "<li><strong>turnover_ratio:</strong> " + String(t.turnover_ratio, 3) + " → pérdidas por posesión (menos = menos errores)</li>";
    html += "<li><strong>true_shooting_pct:</strong> " + String(t.true_shooting_pct * 100, 2) + "% → eficiencia real de tiro (con tiros libres y triples)</li>";
    html += "<li><strong>rebound_pct:</strong> " + String(t.rebound_pct * 100, 2) + "% → porcentaje de rebotes posibles capturados</li>";
    html += "<li><strong>net_rating:</strong> " + String(t.net_rating, 2) + " → rating ofensivo menos defensivo (diferencia de calidad)</li>";
    html += "<li><strong>elo_like:</strong> " + String(t.elo_like, 2) + " → estimación tipo ELO (historial + stats clave)</li>";
    html += "<li><strong>raptor_like:</strong> " + String(t.raptor_like, 2) + " → estimación tipo RAPTOR (impacto ofensivo y defensivo)</li>";
    html += "</ul>";
    return html;
  };

  String result = "<!DOCTYPE html><html><head><meta charset='utf-8'><title>Resultado</title></head><body>";
  result += "<h1>Resultado de Predicción</h1>";
  result += "<p><strong>" + A.name + ":</strong> Score final: " + String(scoreA, 2) + "</p>";
  result += teamToHTML(A);
  result += "<hr>";
  result += "<p><strong>" + B.name + ":</strong> Score final: " + String(scoreB, 2) + "</p>";
  result += teamToHTML(B);
  result += "<hr><h2>";
  if (scoreA > scoreB) result += "✅ Ganador predicho: " + A.name;
  else if (scoreB > scoreA) result += "✅ Ganador predicho: " + B.name;
  else result += "🤝 Empate técnico predicho";
  result += "</h2><br><br><a href='/'>🔙 Nueva predicción</a></body></html>";

  server.send(200, "text/html", result);
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.print("Conectando a WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print('.');
  }
  Serial.println("\nConectado! IP: " + WiFi.localIP().toString());
  server.on("/", HTTP_GET, handleRoot);
  server.on("/predict", HTTP_POST, handlePredict);
  server.begin();
  Serial.println("Servidor iniciado");
}

void loop() {
  server.handleClient();
}











