/*
 * The email server option and the email argument of the query functions. The
 * server below refuses every connection, and the error raised for the failed
 * request names its URL, so the email parameter that was sent can be read
 * from it - no network needed.
 */
CREATE SERVER mail
FOREIGN DATA WRAPPER nominatim_fdw
OPTIONS (url 'http://127.0.0.1:1', max_connect_retry '0', connect_timeout '1');

/* the email parameter of the request, '-' if it had none */
CREATE FUNCTION sent_email(query text) RETURNS text AS $$
DECLARE
    detail text;
BEGIN
    EXECUTE query;
    RETURN NULL;
EXCEPTION WHEN fdw_unable_to_create_execution THEN
    GET STACKED DIAGNOSTICS detail = PG_EXCEPTION_DETAIL;
    RETURN coalesce(substring(detail from '[?&]email=([^&"]*)'), '-');
END;
$$ LANGUAGE plpgsql;

/* valid addresses */
ALTER SERVER mail OPTIONS (ADD email 'ops@acme.example');
ALTER SERVER mail OPTIONS (SET email 'gis.team+bulk@sub.acme-geo.example');
ALTER SERVER mail OPTIONS (SET email 'ops@localhost');
SELECT srvoptions FROM pg_foreign_server WHERE srvname = 'mail';

/* invalid addresses: rejected, the option keeps its value */
ALTER SERVER mail OPTIONS (SET email '');
ALTER SERVER mail OPTIONS (SET email 'ops');
ALTER SERVER mail OPTIONS (SET email '@acme.example');
ALTER SERVER mail OPTIONS (SET email 'ops@');
CREATE SERVER mail2 FOREIGN DATA WRAPPER nominatim_fdw OPTIONS (url 'http://127.0.0.1:1', email 'not-an-address');
SELECT srvoptions FROM pg_foreign_server WHERE srvname = 'mail';

/* email is a server option, not a user mapping option */
CREATE USER MAPPING FOR PUBLIC SERVER mail OPTIONS (email 'ops@acme.example');

/* the server option is sent, a non-empty argument overrides it */
ALTER SERVER mail OPTIONS (SET email 'ops@acme.example');
SELECT sent_email($$SELECT * FROM nominatim_search('mail', q => 'Muenster')$$);
SELECT sent_email($$SELECT * FROM nominatim_search('mail', q => 'Muenster', email => 'gis+bulk@acme.example')$$);
SELECT sent_email($$SELECT * FROM nominatim_search('mail', q => 'Muenster', email => '')$$);
SELECT sent_email($$SELECT * FROM nominatim_lookup('mail', 'R62591')$$);
SELECT sent_email($$SELECT * FROM nominatim_reverse('mail', 7.6, 51.9)$$);

/* without the option, only the argument is sent */
ALTER SERVER mail OPTIONS (DROP email);
SELECT sent_email($$SELECT * FROM nominatim_search('mail', q => 'Muenster')$$);
SELECT sent_email($$SELECT * FROM nominatim_reverse('mail', 7.6, 51.9, email => 'gis@acme.example')$$);

DROP FUNCTION sent_email(text);
DROP SERVER mail;
