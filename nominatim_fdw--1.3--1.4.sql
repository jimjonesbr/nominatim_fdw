/* Fix incorrect IMMUTABLE volatility on functions that make HTTP requests */
ALTER FUNCTION nominatim_search(text, text, text, text, text, text, text, text, text, boolean, boolean, boolean, text, text, text, text, text, text, text, boolean, double precision, text, boolean, int, int) VOLATILE;
ALTER FUNCTION nominatim_lookup(text, text, boolean, boolean, boolean, text, text, text, text, text, text, text, boolean, double precision, text, boolean) VOLATILE;
ALTER FUNCTION nominatim_reverse(text, double precision, double precision, int, text, boolean, boolean, boolean, text, text) VOLATILE;
