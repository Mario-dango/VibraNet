-- =========================================================
-- Proyecto: Plataforma IoT Sísmica (multi-cliente/tenant)
-- Modelo lógico en 3NF con M:N usuario<->sistema
-- MySQL 8.0 / InnoDB / utf8mb4
-- =========================================================
SET NAMES utf8mb4;

CREATE DATABASE IF NOT EXISTS seismic_iot
  DEFAULT CHARACTER SET utf8mb4
  COLLATE utf8mb4_0900_ai_ci;

USE seismic_iot;
SET SESSION sql_mode = 'STRICT_TRANS_TABLES,NO_ENGINE_SUBSTITUTION';

-- Guardar estado previo
SET @OLD_SQL_MODE := @@SQL_MODE;
SET @OLD_FOREIGN_KEY_CHECKS := @@FOREIGN_KEY_CHECKS;

-- SQL mode recomendado
SET SESSION sql_mode = 'STRICT_TRANS_TABLES,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION';

-- Seguridad para DDL en bloque
SET FOREIGN_KEY_CHECKS = 0;

START TRANSACTION;

-- =========================================================
-- Tabla: cliente
-- =========================================================
CREATE TABLE IF NOT EXISTS cliente (
  cliente_id   BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT 'PK surrogate',
  cliente_code VARCHAR(64) NOT NULL COMMENT 'Código único de cliente',
  nombre       VARCHAR(160) NOT NULL COMMENT 'Nombre comercial / razón social',
  max_usuarios INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Tope contractual de usuarios a nivel cliente',
  created_at   DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  updated_at   DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
  deleted_at   DATETIME(3) NULL,
  PRIMARY KEY (cliente_id),
  UNIQUE KEY uk_cliente_code (cliente_code),
  UNIQUE KEY uk_cliente_nombre (nombre),
  CHECK (max_usuarios >= 0)
) ENGINE=InnoDB ROW_FORMAT=DYNAMIC COMMENT='Tenant/empresa contratante';

-- =========================================================
-- Tabla: usuario
-- =========================================================
CREATE TABLE IF NOT EXISTS usuario (
  usuario_id    BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT 'PK surrogate',
  cliente_id    BIGINT UNSIGNED NOT NULL COMMENT 'FK a cliente',
  nombre        VARCHAR(80)  NOT NULL,
  apellido      VARCHAR(80)  NOT NULL,
  correo        VARCHAR(160) NOT NULL COMMENT 'Único por usuario',
  password_hash VARCHAR(255) NOT NULL COMMENT 'Hash (no guardar password en claro)',
  created_at    DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  updated_at    DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
  deleted_at    DATETIME(3) NULL,
  PRIMARY KEY (usuario_id),
  UNIQUE KEY uk_usuario_correo (correo),
  KEY idx_usuario_cliente (cliente_id),
  CONSTRAINT fk_usuario_cliente
    FOREIGN KEY (cliente_id) REFERENCES cliente(cliente_id)
    ON UPDATE CASCADE
    ON DELETE RESTRICT
) ENGINE=InnoDB ROW_FORMAT=DYNAMIC COMMENT='Identidad de acceso, asociada a un cliente';

-- =========================================================
-- Tabla: sistema
-- =========================================================
CREATE TABLE IF NOT EXISTS sistema (
  sistema_id            BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT 'PK surrogate',
  cliente_id            BIGINT UNSIGNED NOT NULL COMMENT 'FK a cliente (tenant owner)',
  sistema_code          VARCHAR(64) NOT NULL COMMENT 'Código único del sistema',
  plan                  VARCHAR(64) NOT NULL COMMENT 'Nombre del plan',
  costo                 DECIMAL(12,2) NOT NULL DEFAULT 0.00 COMMENT 'Costo del plan/mes o tarifa',
  max_usuarios_override INT UNSIGNED NULL COMMENT 'Límite opcional por sistema; si NULL, rige cliente.max_usuarios',
  created_at            DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  updated_at            DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
  deleted_at            DATETIME(3) NULL,
  PRIMARY KEY (sistema_id),
  UNIQUE KEY uk_sistema_code (sistema_code),
  KEY idx_sistema_cliente (cliente_id),
  CONSTRAINT fk_sistema_cliente
    FOREIGN KEY (cliente_id) REFERENCES cliente(cliente_id)
    ON UPDATE CASCADE
    ON DELETE RESTRICT,
  CHECK (costo >= 0.00)
) ENGINE=InnoDB ROW_FORMAT=DYNAMIC COMMENT='Instancia operativa del cliente';

-- =========================================================
-- Tabla puente: usuario_sistema (accesos M:N)
-- =========================================================
CREATE TABLE IF NOT EXISTS usuario_sistema (
  usuario_id     BIGINT UNSIGNED NOT NULL COMMENT 'FK a usuario',
  sistema_id     BIGINT UNSIGNED NOT NULL COMMENT 'FK a sistema',
  rol            ENUM('OWNER','ADMIN','EDITOR','VIEWER') NOT NULL DEFAULT 'VIEWER' COMMENT 'Rol de acceso',
  estado_acceso  ENUM('ACTIVO','SUSPENDIDO') NOT NULL DEFAULT 'ACTIVO',
  created_at     DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  updated_at     DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
  PRIMARY KEY (usuario_id, sistema_id),
  KEY idx_us_sistema (sistema_id),
  CONSTRAINT fk_us_usuario
    FOREIGN KEY (usuario_id) REFERENCES usuario(usuario_id)
    ON UPDATE CASCADE
    ON DELETE CASCADE,
  CONSTRAINT fk_us_sistema
    FOREIGN KEY (sistema_id) REFERENCES sistema(sistema_id)
    ON UPDATE CASCADE
    ON DELETE CASCADE,
  CHECK ( (rol IS NOT NULL) AND (estado_acceso IS NOT NULL) )
) ENGINE=InnoDB ROW_FORMAT=DYNAMIC COMMENT='Accesos por sistema, con rol/estado';

-- =========================================================
-- Tabla: nodo
-- =========================================================
CREATE TABLE IF NOT EXISTS nodo (
  nodo_id                  BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT 'PK surrogate',
  sistema_id               BIGINT UNSIGNED NOT NULL COMMENT 'FK a sistema',
  nodo_code                VARCHAR(64) NOT NULL COMMENT 'Código único del nodo (preferible único por sistema)',
  lugar_instalacion        VARCHAR(160) NULL,
  tecnico_instalacion      VARCHAR(120) NULL,
  modelo                   VARCHAR(80)  NULL,
  version                  VARCHAR(40)  NULL,
  ultimo_keepalive_utc     DATETIME(3) NULL COMMENT 'Último ping del nodo (UTC)',
  keepalive_interval_s     INT UNSIGNED NOT NULL DEFAULT 300 COMMENT 'Intervalo esperado de keepalive (segundos)',
  habilitado_para_mediciones TINYINT(1) NOT NULL DEFAULT 1 COMMENT '1=habilitado, 0=no',
  created_at               DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  updated_at               DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
  deleted_at               DATETIME(3) NULL,
  PRIMARY KEY (nodo_id),
  UNIQUE KEY uk_nodo_code (nodo_code),
  KEY idx_nodo_sistema (sistema_id),
  KEY idx_nodo_keepalive (sistema_id, ultimo_keepalive_utc),
  CONSTRAINT fk_nodo_sistema
    FOREIGN KEY (sistema_id) REFERENCES sistema(sistema_id)
    ON UPDATE CASCADE
    ON DELETE RESTRICT,
  CHECK (keepalive_interval_s >= 0)
) ENGINE=InnoDB ROW_FORMAT=DYNAMIC COMMENT='Equipo en campo dentro de un sistema';

-- =========================================================
-- Tabla: mediciones
-- =========================================================
CREATE TABLE IF NOT EXISTS mediciones (
  medicion_id  BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT 'PK surrogate',
  nodo_id      BIGINT UNSIGNED NOT NULL COMMENT 'FK a nodo',
  measured_at  DATETIME(3) NOT NULL COMMENT 'Timestamp UTC de la lectura',
  payload_json JSON NOT NULL COMMENT 'Valores medidos (claves estables: ej. ax, ay, az, batt, temp)',
  created_at   DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  updated_at   DATETIME(3) NOT NULL DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
  deleted_at   DATETIME(3) NULL,
  PRIMARY KEY (medicion_id),
  KEY idx_med_nodo_time (nodo_id, measured_at),
  KEY idx_med_time (measured_at),
  CONSTRAINT fk_med_nodo
    FOREIGN KEY (nodo_id) REFERENCES nodo(nodo_id)
    ON UPDATE CASCADE
    ON DELETE RESTRICT,
  CHECK (JSON_VALID(payload_json))
) ENGINE=InnoDB ROW_FORMAT=DYNAMIC COMMENT='Lecturas por nodo';

-- =========================================================
-- Triggers de integridad de negocio
-- 1) usuario_sistema: mismo cliente entre usuario y sistema
-- 2) usuario_sistema: respetar tope de usuarios (override o de cliente)
-- 3) mediciones: solo si nodo.habilitado_para_mediciones = 1
-- =========================================================

-- 1) Mismo cliente (INSERT)
DROP TRIGGER IF EXISTS trg_us_same_client_bi;
DELIMITER //
CREATE TRIGGER trg_us_same_client_bi
BEFORE INSERT ON usuario_sistema
FOR EACH ROW
BEGIN
  DECLARE v_usuario_cliente BIGINT UNSIGNED;
  DECLARE v_sistema_cliente BIGINT UNSIGNED;
  SELECT u.cliente_id INTO v_usuario_cliente FROM usuario u WHERE u.usuario_id = NEW.usuario_id;
  SELECT s.cliente_id INTO v_sistema_cliente FROM sistema s WHERE s.sistema_id = NEW.sistema_id;
  IF v_usuario_cliente IS NULL OR v_sistema_cliente IS NULL OR v_usuario_cliente <> v_sistema_cliente THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Usuario y Sistema pertenecen a distinto cliente';
  END IF;
END//
DELIMITER ;

-- 1b) Mismo cliente (UPDATE)
DROP TRIGGER IF EXISTS trg_us_same_client_bu;
DELIMITER //
CREATE TRIGGER trg_us_same_client_bu
BEFORE UPDATE ON usuario_sistema
FOR EACH ROW
BEGIN
  DECLARE v_usuario_cliente BIGINT UNSIGNED;
  DECLARE v_sistema_cliente BIGINT UNSIGNED;
  SELECT u.cliente_id INTO v_usuario_cliente FROM usuario u WHERE u.usuario_id = NEW.usuario_id;
  SELECT s.cliente_id INTO v_sistema_cliente FROM sistema s WHERE s.sistema_id = NEW.sistema_id;
  IF v_usuario_cliente IS NULL OR v_sistema_cliente IS NULL OR v_usuario_cliente <> v_sistema_cliente THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Usuario y Sistema pertenecen a distinto cliente';
  END IF;
END//
DELIMITER ;

-- 2) Tope de usuarios por sistema (INSERT)
DROP TRIGGER IF EXISTS trg_us_cap_bi;
DELIMITER //
CREATE TRIGGER trg_us_cap_bi
BEFORE INSERT ON usuario_sistema
FOR EACH ROW
BEGIN
  DECLARE v_cliente_id BIGINT UNSIGNED;
  DECLARE v_cap_cliente INT;
  DECLARE v_cap_sistema INT;
  DECLARE v_cap_efectivo INT;
  DECLARE v_usuarios_actuales INT;

  SELECT s.cliente_id, COALESCE(s.max_usuarios_override, c.max_usuarios)
  INTO v_cliente_id, v_cap_efectivo
  FROM sistema s JOIN cliente c ON c.cliente_id = s.cliente_id
  WHERE s.sistema_id = NEW.sistema_id;

  -- contar accesos ACTIVO al sistema
  SELECT COUNT(*)
    INTO v_usuarios_actuales
    FROM usuario_sistema us
   WHERE us.sistema_id = NEW.sistema_id
     AND us.estado_acceso = 'ACTIVO';

  IF NEW.estado_acceso = 'ACTIVO' AND v_cap_efectivo IS NOT NULL AND v_cap_efectivo > 0
     AND v_usuarios_actuales >= v_cap_efectivo THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Capacidad de usuarios del sistema alcanzada';
  END IF;
END//
DELIMITER ;

-- 2b) Tope de usuarios por sistema (UPDATE)
DROP TRIGGER IF EXISTS trg_us_cap_bu;
DELIMITER //
CREATE TRIGGER trg_us_cap_bu
BEFORE UPDATE ON usuario_sistema
FOR EACH ROW
BEGIN
  DECLARE v_cap_efectivo INT;
  DECLARE v_usuarios_actuales INT;

  SELECT COALESCE(s.max_usuarios_override, c.max_usuarios)
    INTO v_cap_efectivo
    FROM sistema s JOIN cliente c ON c.cliente_id = s.cliente_id
   WHERE s.sistema_id = NEW.sistema_id;

  -- Si pasa a ACTIVO, verificamos conteo (excluye al propio OLD si ya estaba ACTIVO)
  IF NEW.estado_acceso = 'ACTIVO' THEN
    SELECT COUNT(*)
      INTO v_usuarios_actuales
      FROM usuario_sistema us
     WHERE us.sistema_id = NEW.sistema_id
       AND us.estado_acceso = 'ACTIVO'
       AND NOT (us.usuario_id = OLD.usuario_id AND us.sistema_id = OLD.sistema_id);

    IF v_cap_efectivo IS NOT NULL AND v_cap_efectivo > 0
       AND v_usuarios_actuales >= v_cap_efectivo THEN
      SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Capacidad de usuarios del sistema alcanzada';
    END IF;
  END IF;
END//
DELIMITER ;

-- 3) Mediciones solo si nodo habilitado
DROP TRIGGER IF EXISTS trg_med_habilitado_bi;
DELIMITER //
CREATE TRIGGER trg_med_habilitado_bi
BEFORE INSERT ON mediciones
FOR EACH ROW
BEGIN
  DECLARE v_hab TINYINT;
  SELECT n.habilitado_para_mediciones INTO v_hab FROM nodo n WHERE n.nodo_id = NEW.nodo_id;
  IF v_hab IS NULL OR v_hab = 0 THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Nodo no habilitado para recibir mediciones';
  END IF;
END//
DELIMITER ;

COMMIT;

-- Restaurar estado
SET FOREIGN_KEY_CHECKS = @OLD_FOREIGN_KEY_CHECKS;
SET SESSION SQL_MODE = @OLD_SQL_MODE;
