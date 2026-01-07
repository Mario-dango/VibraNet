 
-- ============ nodo ============
CREATE TABLE IF NOT EXISTS nodo (
  nodo_id                    BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  sistema_id                 BIGINT UNSIGNED NOT NULL,
  nodo_code                  VARCHAR(64) NOT NULL,
  lugar_instalacion          VARCHAR(160) NULL,
  tecnico_instalacion        VARCHAR(120) NULL,
  modelo                     VARCHAR(80)  NULL,
  version                    VARCHAR(40)  NULL,
  ultimo_keepalive_utc       DATETIME(3) NULL,
  keepalive_interval_s       INT UNSIGNED NOT NULL DEFAULT 300,
  habilitado_para_mediciones TINYINT NOT NULL DEFAULT 1,
  created_at                 DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  updated_at                 DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
  deleted_at                 DATETIME(3) NULL,
  PRIMARY KEY (nodo_id),
  UNIQUE KEY uk_nodo_code (nodo_code),
  KEY idx_nodo_sistema (sistema_id),
  KEY idx_nodo_keepalive (sistema_id, ultimo_keepalive_utc),
  CHECK (keepalive_interval_s >= 0)
) ENGINE=InnoDB ROW_FORMAT=DYNAMIC COMMENT='Equipo en campo';

-- ============ mediciones ============
CREATE TABLE IF NOT EXISTS mediciones (
  medicion_id  BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  nodo_id      BIGINT UNSIGNED NOT NULL,
  measured_at  DATETIME(3) NOT NULL,
  payload_json JSON NOT NULL,
  created_at   DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  updated_at   DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
  deleted_at   DATETIME(3) NULL,
  PRIMARY KEY (medicion_id),
  KEY idx_med_nodo_time (nodo_id, measured_at),
  KEY idx_med_time (measured_at),
  CHECK (JSON_VALID(payload_json))
) ENGINE=InnoDB ROW_FORMAT=DYNAMIC COMMENT='Lecturas por nodo';
