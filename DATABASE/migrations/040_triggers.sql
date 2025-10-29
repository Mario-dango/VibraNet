-- Mismo cliente en usuario_sistema (INSERT/UPDATE)
DROP TRIGGER IF EXISTS trg_us_same_client_bi;
DELIMITER //
CREATE TRIGGER trg_us_same_client_bi
BEFORE INSERT ON usuario_sistema
FOR EACH ROW
BEGIN
  DECLARE v_u BIGINT UNSIGNED; DECLARE v_s BIGINT UNSIGNED;
  SELECT cliente_id INTO v_u FROM usuario WHERE usuario_id=NEW.usuario_id;
  SELECT cliente_id INTO v_s FROM sistema WHERE sistema_id=NEW.sistema_id;
  IF v_u IS NULL OR v_s IS NULL OR v_u<>v_s THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Usuario y Sistema de distinto cliente';
  END IF;
END//
DELIMITER ;

DROP TRIGGER IF EXISTS trg_us_same_client_bu;
DELIMITER //
CREATE TRIGGER trg_us_same_client_bu
BEFORE UPDATE ON usuario_sistema
FOR EACH ROW
BEGIN
  DECLARE v_u BIGINT UNSIGNED; DECLARE v_s BIGINT UNSIGNED;
  SELECT cliente_id INTO v_u FROM usuario WHERE usuario_id=NEW.usuario_id;
  SELECT cliente_id INTO v_s FROM sistema WHERE sistema_id=NEW.sistema_id;
  IF v_u IS NULL OR v_s IS NULL OR v_u<>v_s THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Usuario y Sistema de distinto cliente';
  END IF;
END//
DELIMITER ;

-- Tope de usuarios activos por sistema (INSERT/UPDATE)
DROP TRIGGER IF EXISTS trg_us_cap_bi;
DELIMITER //
CREATE TRIGGER trg_us_cap_bi
BEFORE INSERT ON usuario_sistema
FOR EACH ROW
BEGIN
  DECLARE v_cap INT; DECLARE v_cnt INT;
  SELECT COALESCE(s.max_usuarios_override, c.max_usuarios)
    INTO v_cap
    FROM sistema s JOIN cliente c ON c.cliente_id=s.cliente_id
   WHERE s.sistema_id=NEW.sistema_id;

  SELECT COUNT(*) INTO v_cnt
    FROM usuario_sistema WHERE sistema_id=NEW.sistema_id AND estado_acceso='ACTIVO';

  IF NEW.estado_acceso='ACTIVO' AND v_cap IS NOT NULL AND v_cap>0 AND v_cnt>=v_cap THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Capacidad de usuarios del sistema alcanzada';
  END IF;
END//
DELIMITER ;

DROP TRIGGER IF EXISTS trg_us_cap_bu;
DELIMITER //
CREATE TRIGGER trg_us_cap_bu
BEFORE UPDATE ON usuario_sistema
FOR EACH ROW
BEGIN
  DECLARE v_cap INT; DECLARE v_cnt INT;
  SELECT COALESCE(s.max_usuarios_override, c.max_usuarios)
    INTO v_cap
    FROM sistema s JOIN cliente c ON c.cliente_id=s.cliente_id
   WHERE s.sistema_id=NEW.sistema_id;

  IF NEW.estado_acceso='ACTIVO' THEN
    SELECT COUNT(*) INTO v_cnt
      FROM usuario_sistema
     WHERE sistema_id=NEW.sistema_id AND estado_acceso='ACTIVO'
       AND NOT (usuario_id=OLD.usuario_id AND sistema_id=OLD.sistema_id);
    IF v_cap IS NOT NULL AND v_cap>0 AND v_cnt>=v_cap THEN
      SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Capacidad de usuarios del sistema alcanzada';
    END IF;
  END IF;
END//
DELIMITER ;

-- Mediciones solo si nodo habilitado
DROP TRIGGER IF EXISTS trg_med_habilitado_bi;
DELIMITER //
CREATE TRIGGER trg_med_habilitado_bi
BEFORE INSERT ON mediciones
FOR EACH ROW
BEGIN
  DECLARE v_hab TINYINT;
  SELECT habilitado_para_mediciones INTO v_hab FROM nodo WHERE nodo_id=NEW.nodo_id;
  IF v_hab IS NULL OR v_hab=0 THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Nodo no habilitado para recibir mediciones';
  END IF;
END//
DELIMITER ;
