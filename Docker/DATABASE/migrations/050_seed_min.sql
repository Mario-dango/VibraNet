-- Seed mínimo de ejemplo
INSERT IGNORE INTO cliente (cliente_code, nombre, max_usuarios)
VALUES ('CL-MZA-001','Cliente Mendoza SA',25);

INSERT INTO usuario (cliente_id, nombre, apellido, correo, password_hash)
SELECT c.cliente_id, 'Ana','García','ana.garcia@example.com','hash$argon2id$...' 
FROM cliente c WHERE c.cliente_code='CL-MZA-001'
ON DUPLICATE KEY UPDATE updated_at=CURRENT_TIMESTAMP(3);

INSERT INTO sistema (cliente_id, sistema_code, plan, costo, max_usuarios_override)
SELECT c.cliente_id,'SYS-MZA-BASE','BASE',0.00,NULL
FROM cliente c WHERE c.cliente_code='CL-MZA-001'
ON DUPLICATE KEY UPDATE updated_at=CURRENT_TIMESTAMP(3);

INSERT INTO usuario_sistema (usuario_id, sistema_id, rol, estado_acceso)
SELECT u.usuario_id, s.sistema_id, 'ADMIN','ACTIVO'
FROM usuario u JOIN sistema s ON s.cliente_id=u.cliente_id
WHERE u.correo='ana.garcia@example.com' AND s.sistema_code='SYS-MZA-BASE'
ON DUPLICATE KEY UPDATE updated_at=CURRENT_TIMESTAMP(3);

INSERT INTO nodo (sistema_id, nodo_code, lugar_instalacion, modelo, version, keepalive_interval_s, habilitado_para_mediciones)
SELECT s.sistema_id,'NODO-Z-001','Mendoza Centro','SeismoBox v2','1.4.3',300,1
FROM sistema s WHERE s.sistema_code='SYS-MZA-BASE'
ON DUPLICATE KEY UPDATE updated_at=CURRENT_TIMESTAMP(3);

INSERT INTO mediciones (nodo_id, measured_at, payload_json)
SELECT n.nodo_id, NOW(3), JSON_OBJECT('ax',0.0123,'ay',-0.004,'az',0.001,'batt',3.92,'temp',24.1)
FROM nodo n WHERE n.nodo_code='NODO-Z-001';
