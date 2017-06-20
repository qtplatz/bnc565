
function downloadOnload() {

    loadBanner();
    
    $("#p\\[0\\]").load( "protocol.html section" );
//    $("#p\\[3\\]").load( "protocol.html section", function(e){
//	$("div #protocolIndex").each( function( index ){
//	    $(this).text( "Protocol# " + ( index + 1 ) );
//	});
//    } );

    fetchStatus();
}

if ( window.addEventListener )
    window.addEventListener( "load", downloadOnload );
else if ( window.attachEvent)
    window.attachEvent( "load", downloadOnload);
