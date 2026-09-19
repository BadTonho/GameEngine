struct EditorUiVertexInput
{
    [[vk::location(0)]] float2 position;
    [[vk::location(1)]] float2 uv;
    [[vk::location(2)]] float4 color;
};

struct EditorUiVertexOutput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float2 uv;
    [[vk::location(1)]] float4 color;
};

[shader("vertex")]
EditorUiVertexOutput editor_ui_vertex_main(EditorUiVertexInput input)
{
    EditorUiVertexOutput output;
    output.position = float4(input.position, 0.0, 1.0);
    output.uv = input.uv;
    output.color = input.color;
    return output;
}

struct EditorUiFragmentInput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float2 uv;
    [[vk::location(1)]] float4 color;
};

[shader("fragment")]
float4 editor_ui_fragment_main(EditorUiFragmentInput input) : SV_Target0
{
    return input.color;
}
