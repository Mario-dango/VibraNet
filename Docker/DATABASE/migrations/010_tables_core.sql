 -- ============ cliente ============
CREATE TABLE IF NOT EXISTS cliente (
  cliente_id   BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  cliente_code VARCHAR(64) NOT NULL,
  nombre       VARCHAR(160) NOT NULL,
  max_usuarios INT UNSIGNED NOT NULL DEFAULT 0,
  created_at   DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  updated_at   DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
  deleted_at   DATETIME(3) NULL,
  PRIMARY KEY (cliente_id),
  UNIQUE KEY uk_cliente_code (cliente_code),
  UNIQUE KEY uk_cliente_nombre (nombre),
  CHECK (max_usuarios >= 0)
) ENGINE=InnoDB ROW_FORMAT=DYNAMIC COMMENT='Tenant/empresa';

-- ============ usuario ============
CREATE TABLE IF NOT EXISTS usuario (
  usuario_id    BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  cliente_id    BIGINT UNSIGNED NOT NULL,
  nombre        VARCHAR(80)  NOT NULL,
  apellido      VARCHAR(80)  NOT NULL,
  correo        VARCHAR(160) NOT NULL,
  password_hash VARCHAR(255) NOT NULL,
  created_at    DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  updated_at    DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
  deleted_at    DATETIME(3) NULL,
  PRIMARY KEY (usuario_id),
  UNIQUE KEY uk_usuario_correo (correo),
  KEY idx_usuario_cliente (cliente_id)
) ENGINE=InnoDB ROW_FORMAT=DYNAMIC COMMENT='Usuario del cliente';

-- ============ sistema ============
CREATE TABLE IF NOT EXISTS sistema (
  sistema_id            BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  cliente_id            BIGINT UNSIGNED NOT NULL,
  sistema_code          VARCHAR(64) NOT NULL,
  plan                  VARCHAR(64) NOT NULL,
  costo                 DECIMAL(12,2) NOT NULL DEFAULT 0.00,
  max_usuarios_override INT UNSIGNED NULL,
  created_at            DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  updated_at            DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
  deleted_at            DATETIME(3) NULL,
  PRIMARY KEY (sistema_id),
  UNIQUE KEY uk_sistema_code (sistema_code),
  KEY idx_sistema_cliente (cliente_id),
  CHECK (costo >= 0.00)
) ENGINE=InnoDB ROW_FORMAT=DYNAMIC COMMENT='Instancia del cliente';

-- ============ usuario_sistema (M:N) ============
CREATE TABLE IF NOT EXISTS usuario_sistema (
  usuario_id     BIGINT UNSIGNED NOT NULL,
  sistema_id     BIGINT UNSIGNED NOT NULL,
  rol            ENUM('OWNER','ADMIN','EDITOR','VIEWER') NOT NULL DEFAULT 'VIEWER',
  estado_acceso  ENUM('ACTIVO','SUSPENDIDO') NOT NULL DEFAULT 'ACTIVO',
  created_at     DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  updated_at     DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
  PRIMARY KEY (usuario_id, sistema_id),
  KEY idx_us_sistema (sistema_id)
) ENGINE=InnoDB ROW_FORMAT=DYNAMIC COMMENT='Accesos por sistema';

