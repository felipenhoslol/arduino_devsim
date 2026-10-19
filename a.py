import json
import sqlite3
import logging
from datetime import datetime
import paho.mqtt.client as mqtt

# ==============================
# CONFIGURACAO
# ==============================

BROKER = "127.0.0.1"
PORTA = 1883

TOPICO_DADOS = "smartfactory/node1/telemetria"
TOPICO_COMANDOS = "smartfactory/node1/comandos"

TOKEN_ESPERADO = "TOKEN_DE_TESTE"
ID_ESPERADO = "NODE1"

# ==============================
# LOGS
# ==============================

logging.basicConfig(
    filename="seguranca.log",
    level=logging.INFO,
    format="%(asctime)s %(message)s"
)

anomalias = logging.getLogger("anomalias")
anomalias.setLevel(logging.WARNING)

arquivo_anomalias = logging.FileHandler(
    "anomalias.log",
    encoding="utf-8"
)

arquivo_anomalias.setFormatter(
    logging.Formatter("%(asctime)s %(message)s")
)

anomalias.addHandler(arquivo_anomalias)

# ==============================
# BANCO DE DADOS
# ==============================

conexao = sqlite3.connect(
    "dados_sensores.db",
    check_same_thread=False
)

cursor = conexao.cursor()

cursor.execute("""
CREATE TABLE IF NOT EXISTS leituras (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp TEXT,
    id_no TEXT,
    temperatura REAL,
    umidade REAL,
    chama INTEGER,
    vibracoes INTEGER,
    barreira_ok INTEGER,
    estado TEXT,
    rele INTEGER
)
""")

conexao.commit()

# ==============================
# RECEBIMENTO MQTT
# ==============================

def ao_conectar(cliente, userdata, flags, rc):
    if rc == 0:
        print("Conectado ao broker MQTT.")
        cliente.subscribe(TOPICO_DADOS)
    else:
        print("Falha ao conectar ao broker:", rc)


def ao_receber(cliente, userdata, mensagem):
    origem = mensagem.topic

    try:
        dados = json.loads(
            mensagem.payload.decode("utf-8")
        )

        if not isinstance(dados, dict):
            raise ValueError("JSON deve ser um objeto.")

        if dados.get("id_no") != ID_ESPERADO:
            logging.warning(
                "Origem=%s motivo=NO_DESCONHECIDO",
                origem
            )
            return

        if dados.get("token") != TOKEN_ESPERADO:
            logging.warning(
                "Origem=%s motivo=TOKEN_INVALIDO",
                origem
            )
            return

        temperatura = float(dados["temperatura"])
        umidade = float(dados["umidade"])
        chama = bool(dados["chama"])
        vibracoes = int(dados["vibracoes"])
        barreira_ok = bool(dados["barreira_ok"])
        estado = str(dados["estado"])
        rele = bool(dados["rele"])

        timestamp = datetime.now().isoformat()

        cursor.execute("""
        INSERT INTO leituras (
            timestamp,
            id_no,
            temperatura,
            umidade,
            chama,
            vibracoes,
            barreira_ok,
            estado,
            rele
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
        """, (
            timestamp,
            ID_ESPERADO,
            temperatura,
            umidade,
            int(chama),
            vibracoes,
            int(barreira_ok),
            estado,
            int(rele)
        ))

        conexao.commit()

        print(
            f"{timestamp} | "
            f"T={temperatura:.1f} C | "
            f"UR={umidade:.1f}% | "
            f"Estado={estado}"
        )

        # Registrar anomalias.
        if temperatura > 30:
            anomalias.warning(
                "CRITICO sensor=temperatura valor=%.1f",
                temperatura
            )

        if umidade < 40 or umidade > 70:
            anomalias.warning(
                "ALERTA sensor=umidade valor=%.1f",
                umidade
            )

        if chama:
            anomalias.warning(
                "CRITICO sensor=chama valor=DETECTADA"
            )

        if not barreira_ok:
            anomalias.warning(
                "VIOLACAO_NR12 sensor=barreira valor=INTERROMPIDA"
            )

    except (ValueError, TypeError, KeyError) as erro:
        logging.warning(
            "Origem=%s motivo=PACOTE_INVALIDO detalhe=%s",
            origem,
            erro
        )

    except Exception as erro:
        logging.exception(
            "Erro ao processar mensagem de %s: %s",
            origem,
            erro
        )


cliente = mqtt.Client(
    callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
    client_id="central_smartfactory"
)

cliente.on_connect = ao_conectar
cliente.on_message = ao_receber

cliente.connect(BROKER, PORTA, 60)

print("Estacao central iniciada.")

cliente.loop_forever()