
function downloadOnload() {

    loadBanner();

    // $('.class-check').bootstrapTobble();
    
    $("#p\\[0\\]").load( "protocol.html section" );

    fetchStatus();
}

if ( window.addEventListener )
    window.addEventListener( "load", downloadOnload );
else if ( window.attachEvent)
    window.attachEvent( "load", downloadOnload);
