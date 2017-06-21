var source = new EventSource( '/dg/ctl?events' );

source.addEventListener( 'tick', function( e ) {
    var json = JSON.parse( e.data );
    if ( json.state ) {
	$('#tick').text( json.state.tick + "/" + ( json.state.state == 0 ? "off" : "on" ));

	var state = json.state.state == 0 ? false : true;
	var cbx = $('#switch-connect');
	if ( state != cbx.prop('checked') )
	    cbx.bootstrapToggle( state ? "on" : "off" );
    }
}, false );

source.onmessage = function(e) {
    var ev = document.getElementById( 'status' )
    if ( ev )
	ev.innerHTML = e.data
};

source.onerror = function( e ) {
    console.log( e )
};

source.onopen = function( e ) {
    var ev = document.getElementById('status')
    if ( ev )
	ev.innerHTML = "open";
}
